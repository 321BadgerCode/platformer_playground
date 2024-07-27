#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <SOIL/SOIL.h>
#include <fstream>
#include <regex>
#include <iostream>
#include <vector>
#include <queue>
#include "./json_parser.h"

using namespace std;

typedef unsigned char u8;

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const float PLAYER_SPEED = 5;
const float GRAVITY = 0.2f;
const float JUMP_FORCE = 10;

struct Object {
	float x, y;
	float width, height;
};
struct Player : Object {
	float dx, dy;
	bool onGround;
};
struct Platform : Object {};
struct Checkpoint : Object {};
struct Enemy : Object {
	float dx, dy;
};

Player player;
float spawnX, spawnY;
vector<Platform> platforms;
vector<Checkpoint> checkpoints;
vector<Enemy> enemies;
vector<pair<float, float>> enemySpeeds;
vector<string> levels;
int currentLevel = 0;

void loadExternalData();
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void updatePlayer();
u8 getCollisionDirection(const Object& player, const Object& object);
void die();
void handleCollision();
void updateEnemies();
void renderScene();
vector<Object> aggregateObject(const char* filepath, u8 r, u8 g, u8 b);
void unloadBitmap();
void loadBitmap(const char* filepath);

int main() {
	if (!glfwInit()) {
		cerr << "Failed to initialize GLFW" << endl;
		return -1;
	}

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Platformer", nullptr, nullptr);
	if (!window) {
		cerr << "Failed to create GLFW window" << endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, keyCallback);

	if (glewInit() != GLEW_OK) {
		cerr << "Failed to initialize GLEW" << endl;
		return -1;
	}

	glOrtho(0.0, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0, -1.0, 1.0);

	loadExternalData();

	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);

		updatePlayer();
		updateEnemies();
		handleCollision();
		renderScene();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void loadExternalData() {
	std::ifstream file("./config.json");
	if (!file) {
		std::cerr << "Could not open file." << std::endl;
		exit(1);
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string input = buffer.str();

	Parser parser(input);
	auto root = parser.parse();
	auto _levels = get_if<JsonNode::JsonObject>(&(*root)["levels"]->getValue());
	if (!_levels) {
		cerr << "Expected an array of levels" << endl;
		exit(EXIT_FAILURE);
	}
	for (const auto& level : *_levels) {
		levels.push_back(level.first);
		shared_ptr<JsonNode> _enemySpeeds = (*(*level.second)["enemies"])["speed"];
		auto enemySpeedsArray = get_if<JsonNode::JsonArray>(&_enemySpeeds->getValue());
		if (!enemySpeedsArray) {
			cerr << "Expected an array of enemy speeds" << endl;
			exit(EXIT_FAILURE);
		}
		for (const auto& enemySpeed : *enemySpeedsArray) {
			float _speed = get<double>(enemySpeed->getValue());
			enemySpeeds.push_back({ _speed, _speed });
		}
	}
	loadBitmap(levels[0].c_str());
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	if (key == GLFW_KEY_LEFT) {
		player.dx = (action != GLFW_RELEASE) ? -PLAYER_SPEED : 0.0f;
	} else if (key == GLFW_KEY_RIGHT) {
		player.dx = (action != GLFW_RELEASE) ? PLAYER_SPEED : 0.0f;
	}

	if ((key == GLFW_KEY_SPACE || key == GLFW_KEY_UP) && action == GLFW_PRESS && player.onGround) {
		player.dy = -JUMP_FORCE;
		player.onGround = false;
	}
}

void updatePlayer() {
	player.dy += GRAVITY;
	player.x += player.dx;
	player.y += player.dy;
}

enum class Direction { TOP = 0b0001, BOTTOM = 0b0010, LEFT = 0b0100, RIGHT = 0b1000 };
u8 getCollisionDirection(const Object& player, const Object& object) {
	u8 direction = 0;
	if (player.y + player.height > object.y && player.y < object.y + object.height) {
		if (player.x + player.width > object.x && player.x < object.x + object.width) {
			float overlapX = min(player.x + player.width, object.x + object.width) - max(player.x, object.x);
			float overlapY = min(player.y + player.height, object.y + object.height) - max(player.y, object.y);

			if (overlapX > overlapY) {
				if (player.y < object.y) {
					direction |= static_cast<u8>(Direction::TOP);
				} else {
					direction |= static_cast<u8>(Direction::BOTTOM);
				}
			} else {
				if (player.x < object.x) {
					direction |= static_cast<u8>(Direction::LEFT);
				} else {
					direction |= static_cast<u8>(Direction::RIGHT);
				}
			}
		}
	}
	return direction;
}

void die() {
	player.x = spawnX;
	player.y = spawnY;
	player.dx = 0;
	player.dy = 0;
	player.onGround = false;
}

void handleCollision() {
	if (player.y + player.height > WINDOW_HEIGHT) {
		die();
	}

	if (player.x < 0) {
		player.x = 0;
	} else if (player.x + player.width > WINDOW_WIDTH) {
		player.x = WINDOW_WIDTH - player.width;
	}

	for (const auto& platform : platforms) {
		u8 direction = getCollisionDirection(player, platform);
		if (direction & static_cast<u8>(Direction::TOP)) {
			player.y = platform.y - player.height;
			player.dy = 0;
			player.onGround = true;
		}
		if (direction & static_cast<u8>(Direction::BOTTOM)) {
			player.y = platform.y + platform.height;
			player.dy = -player.dy;
		}
		if (direction & static_cast<u8>(Direction::LEFT)) {
			player.x = platform.x - player.width;
		}
		if (direction & static_cast<u8>(Direction::RIGHT)) {
			player.x = platform.x + platform.width;
		}
	}

	for (const auto& checkpoint : checkpoints) {
		u8 direction = getCollisionDirection(player, checkpoint);
		if (direction != 0b0000) {
			loadBitmap(levels[++currentLevel].c_str());
		}
	}

	for (const auto& enemy : enemies) {
		u8 direction = getCollisionDirection(player, enemy);
		if (direction != 0b0000) {
			die();
		}
	}
}

void updateEnemies() {
	for (auto& enemy : enemies) {
		enemy.x += enemy.dx;
		if (enemy.x < 0 || enemy.x + enemy.width > WINDOW_WIDTH) {
			enemy.dx = -enemy.dx;
		}
		for (const auto& platform : platforms) {
			u8 direction = getCollisionDirection(enemy, platform);
			if (direction & static_cast<u8>(Direction::LEFT)) {
				enemy.dx = -enemy.dx;
			}
			if (direction & static_cast<u8>(Direction::RIGHT)) {
				enemy.dx = -enemy.dx;
			}
		}
	}
}

void renderScene() {
	// vector<vector<Object>> objects(3);
	// objects[0] = reinterpret_cast<vector<Object>&>(platforms);
	// objects[1] = reinterpret_cast<vector<Object>&>(checkpoints);
	// objects[2] = reinterpret_cast<vector<Object>&>(enemies);
	// vector<vector<float>> colors = {
	// 	{ 1.0f, 1.0f, 1.0f },
	// 	{ 0.0f, 1.0f, 0.0f },
	// 	{ 1.0f, 0.0f, 0.0f }
	// };
	// for (const auto& object : objects) {
	// 	glColor3f(colors[&object - &objects[0]][0], colors[&object - &objects[0]][1], colors[&object - &objects[0]][2]);
	// 	for (const auto& obj : object) {
	// 		glBegin(GL_QUADS);
	// 		glVertex2f(obj.x, obj.y);
	// 		glVertex2f(obj.x + obj.width, obj.y);
	// 		glVertex2f(obj.x + obj.width, obj.y + obj.height);
	// 		glVertex2f(obj.x, obj.y + obj.height);
	// 		glEnd();
	// 	}
	// }
	glColor3f(1.0f, 1.0f, 1.0f);
	for (const auto& platform : platforms) {
		glBegin(GL_QUADS);
		glVertex2f(platform.x, platform.y);
		glVertex2f(platform.x + platform.width, platform.y);
		glVertex2f(platform.x + platform.width, platform.y + platform.height);
		glVertex2f(platform.x, platform.y + platform.height);
		glEnd();
	}
	glColor3f(0.0f, 1.0f, 0.0f);
	for (const auto& checkpoint : checkpoints) {
		glBegin(GL_QUADS);
		glVertex2f(checkpoint.x, checkpoint.y);
		glVertex2f(checkpoint.x + checkpoint.width, checkpoint.y);
		glVertex2f(checkpoint.x + checkpoint.width, checkpoint.y + checkpoint.height);
		glVertex2f(checkpoint.x, checkpoint.y + checkpoint.height);
		glEnd();
	}
	glColor3f(1.0f, 0.0f, 0.0f);
	for (const auto& enemy : enemies) {
		glBegin(GL_QUADS);
		glVertex2f(enemy.x, enemy.y);
		glVertex2f(enemy.x + enemy.width, enemy.y);
		glVertex2f(enemy.x + enemy.width, enemy.y + enemy.height);
		glVertex2f(enemy.x, enemy.y + enemy.height);
		glEnd();
	}

	glColor3f(0.0f, 0.0f, 1.0f);
	glBegin(GL_QUADS);
	glVertex2f(player.x, player.y);
	glVertex2f(player.x + player.width, player.y);
	glVertex2f(player.x + player.width, player.y + player.height);
	glVertex2f(player.x, player.y + player.height);
	glEnd();
}

vector<Object> aggregateObject(unsigned char* image, int width, int height, u8 r, u8 g, u8 b) {
	vector<Object> tmpObjects;

	float xScale = WINDOW_WIDTH / width;
	float yScale = WINDOW_HEIGHT / height;

	// use the flood fill algorithm to make the objects as big as they can be
	// NOTE: flood fill algorithm: https://en.wikipedia.org/wiki/Flood_fill
	// for each pixel in the image, if the pixel is the color we are looking for, then we add it to the queue and then we keep going until the queue is empty
	// we keep track of the min and max x and y values to determine the width and height of the object
	// we then add the object to the objects vector
	// we then set the pixels that we have already visited to a different color so that we don't visit them again
	// we then repeat the process until we have visited all the pixels in the image
	// vector<vector<bool>> visited(height, vector<bool>(width, false));
	// for (int y = height - 1; y >= 0; --y) {
	// 	for (int x = 0; x < width; ++x) {
	// 		if (visited[y][x]) {
	// 			continue;
	// 		}
	// 		if (image[(x + y * width) * 3] == r && image[(x + y * width) * 3 + 1] == g && image[(x + y * width) * 3 + 2] == b) {
	// 			vector<pair<int, int>> queue;
	// 			queue.push_back({ x, y });
	// 			int minX = x, maxX = x, minY = y, maxY = y;
	// 			while (!queue.empty()) {
	// 				auto [x, y] = queue.back();
	// 				queue.pop_back();
	// 				if (x < 0 || x >= width || y < 0 || y >= height || visited[y][x] || image[(x + y * width) * 3] != r || image[(x + y * width) * 3 + 1] != g || image[(x + y * width) * 3 + 2] != b) {
	// 					continue;
	// 				}
	// 				visited[y][x] = true;
	// 				minX = min(minX, x);
	// 				maxX = max(maxX, x);
	// 				minY = min(minY, y);
	// 				maxY = max(maxY, y);
	// 				queue.push_back({ x - 1, y });
	// 				queue.push_back({ x + 1, y });
	// 				queue.push_back({ x, y - 1 });
	// 				queue.push_back({ x, y + 1 });
	// 			}
	// 			Object object = { static_cast<float>(minX) * xScale, static_cast<float>(minY) * yScale, (maxX - minX + 1) * xScale, (maxY - minY + 1) * yScale };
	// 			objects.push_back(object);
	// 		}
	// 	}
	// }

	// I wanted to use flood fill but if multiple objects are touching each other, then the flood fill algorithm will treat them as one object, but this doesn't work, since I'm dealing with rectangles, not meshes with various points to make up a shape.
	vector<vector<bool>> visited(height, vector<bool>(width, false));
	for (int y = height - 1; y >= 0; --y) {
		for (int x = 0; x < width; ++x) {
			if (visited[y][x]) {
				continue;
			}
			if (image[(x + y * width) * 3] == r && image[(x + y * width) * 3 + 1] == g && image[(x + y * width) * 3 + 2] == b) {
				int minX = x, maxX = x, minY = y, maxY = y;
				while (x < width && image[(x + y * width) * 3] == r && image[(x + y * width) * 3 + 1] == g && image[(x + y * width) * 3 + 2] == b) {
					for (int i = minY; i <= maxY; ++i) {
						for (int j = minX; j <= maxX; ++j) {
							visited[i][j] = true;
						}
					}
					maxX = x;
					x++;
				}
				Object object = { static_cast<float>(minX) * xScale, static_cast<float>(minY) * yScale, (maxX - minX + 1) * xScale, (maxY - minY + 1) * yScale };
				tmpObjects.push_back(object);
			}
		}
	}

	// TODO: Optimize the algorithm, so I don't have to do this and instead just do it in one pass in the last loop.
		// This sounds like work though, so I'll just do it in a separate loop for now.
	vector<Object> optimizedObjects;
	for (const Object& object : tmpObjects) {
		bool found = false;
		for (Object& optimizedObject : optimizedObjects) {
			if (object.x == optimizedObject.x && object.width == optimizedObject.width && object.y + object.height == optimizedObject.y) {
				optimizedObject.y = object.y;
				optimizedObject.height += object.height;
				found = true;
				break;
			} else if (object.y == optimizedObject.y && object.height == optimizedObject.height && object.x + object.width == optimizedObject.x) {
				optimizedObject.x = object.x;
				optimizedObject.width += object.width;
				found = true;
				break;
			}
		}
		if (!found) {
			optimizedObjects.push_back(object);
		}
	}

	return optimizedObjects;
}

void unloadBitmap() {
	platforms.clear();
	checkpoints.clear();
	enemies.clear();
}

void loadBitmap(const char* filepath) {
	unloadBitmap();

	int width, height;
	unsigned char* image = SOIL_load_image(filepath, &width, &height, 0, SOIL_LOAD_RGB);
	if (!image) {
		cerr << "Failed to load bitmap" << endl;
		exit(EXIT_FAILURE);
	}

	float xScale = WINDOW_WIDTH / width;
	float yScale = WINDOW_HEIGHT / height;

	vector<Object> playerObject = aggregateObject(image, width, height, 0, 0, 255);
	player.x = playerObject[0].x;
	player.y = playerObject[0].y;
	player.width = playerObject[0].width;
	player.height = playerObject[0].height;
	spawnX = player.x;
	spawnY = player.y;
	vector<Object> platformObjects = aggregateObject(image, width, height, 255, 255, 255);
	for (const Platform& platform : reinterpret_cast<const vector<Platform>&>(platformObjects)) {
		platforms.push_back(platform);
	}
	vector<Object> checkpointObjects = aggregateObject(image, width, height, 0, 255, 0);
	for (const Checkpoint& checkpoint : reinterpret_cast<const vector<Checkpoint>&>(checkpointObjects)) {
		checkpoints.push_back(checkpoint);
	}
	vector<Object> enemyObjects = aggregateObject(image, width, height, 255, 0, 0);
	int enemyCount = 0;
	for (const Object& enemy : enemyObjects) {
		Enemy _enemy = { enemy.x, enemy.y, enemy.width, enemy.height, enemySpeeds[enemyCount].first, enemySpeeds[enemyCount].second };
		enemies.push_back(_enemy);
		enemyCount++;
	}

	SOIL_free_image_data(image);
}