import tkinter as tk
from tkinter import colorchooser, filedialog
from PIL import Image

class ArtSoftware:
	def __init__(self, root):
		self.root = root
		self.root.title("Level Editor")

		self.width_var = tk.IntVar(value=10)
		self.height_var = tk.IntVar(value=10)
		self.color_var = tk.StringVar(value="black")

		self.create_ui()
		self.grid = []
		self.last_clicked_cell = None
	
	def create_ui(self):
		tk.Label(self.root, text="Width:").grid(row=0, column=0, padx=5, pady=5)
		tk.Entry(self.root, textvariable=self.width_var).grid(row=0, column=1, padx=5, pady=5)

		tk.Label(self.root, text="Height:").grid(row=1, column=0, padx=5, pady=5)
		tk.Entry(self.root, textvariable=self.height_var).grid(row=1, column=1, padx=5, pady=5)

		tk.Button(self.root, text="Create Grid", command=self.create_grid).grid(row=2, column=0, columnspan=2, padx=5, pady=5)

		self.create_color_palette()

		tk.Button(self.root, text="Save", command=self.save_image).grid(row=3, column=0, columnspan=2, padx=5, pady=5)
		tk.Button(self.root, text="Load", command=self.load_image).grid(row=3, column=2, padx=5, pady=5)
	
	def create_color_palette(self):
		colors = ["black", "white", "red", "orange", "yellow", "green", "blue", "indigo", "violet"]
		row = 4
		col = 0

		tk.Label(self.root, text="Select Color:").grid(row=row, column=col, padx=5, pady=5)
		for color in colors:
			button = tk.Button(self.root, bg=color, width=2, height=1, command=lambda c=color: self.set_color(c))
			button.grid(row=row, column=col+1, padx=5, pady=5)
			col += 1

		tk.Button(self.root, text="Custom", command=self.choose_custom_color).grid(row=row, column=col+1, padx=5, pady=5)

	def set_color(self, color):
		self.color_var.set(color)
	
	def choose_custom_color(self):
		color_code = colorchooser.askcolor(title="Choose color")[1]
		if color_code:
			self.color_var.set(color_code)

	def create_grid(self):
		width = self.width_var.get()
		height = self.height_var.get()
		if (width <= 0 or height <= 0) or (width > 30 or height > 30):
			return
		
		for row in self.grid:
			for cell in row:
				cell.destroy()

		self.grid = []
		for i in range(height):
			row = []
			for j in range(width):
				button = tk.Button(self.root, bg="black", width=2, height=1, command=lambda r=i, c=j: self.change_color(r, c))
				button.grid(row=i+5, column=j)
				row.append(button)
			self.grid.append(row)

	def change_color(self, row, col):
		color = self.color_var.get()
		if self.grid[row][col].cget("bg") == color:
			if self.last_clicked_cell:
				last_row, last_col = self.last_clicked_cell.grid_info()["row"]-5, self.last_clicked_cell.grid_info()["column"]
				for i in range(min(row, last_row), max(row, last_row)+1):
					for j in range(min(col, last_col), max(col, last_col)+1):
						self.grid[i][j].configure(bg=color)
			return
		self.grid[row][col].configure(bg=color)
		self.last_clicked_cell = self.grid[row][col]

	def save_image(self):
		width = len(self.grid[0])
		height = len(self.grid)
		image = Image.new("RGB", (width, height), "black")
		pixels = image.load()

		for i in range(height):
			for j in range(width):
				color = self.grid[i][j].cget("bg")
				pixels[j, i] = self.root.winfo_rgb(color)
		
		file_path = filedialog.asksaveasfilename(defaultextension=".bmp", filetypes=[("Bitmap files", "*.bmp")])
		if file_path:
			image.save(file_path)

	def load_image(self):
		file_path = filedialog.askopenfilename(filetypes=[("Bitmap files", "*.bmp")])
		if file_path:
			image = Image.open(file_path)
			width, height = image.size

			self.width_var.set(width)
			self.height_var.set(height)

			self.create_grid()
			pixels = image.load()

			for i in range(height):
				for j in range(width):
					color = "#%02x%02x%02x" % pixels[j, i][:3]
					self.grid[i][j].configure(bg=color)

if __name__ == "__main__":
	root = tk.Tk()
	app = ArtSoftware(root)
	root.mainloop()