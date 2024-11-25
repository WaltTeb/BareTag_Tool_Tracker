class Anchor:
    def __init__(self, id, x_coord, y_coord):
        self.id = id
        self.x_coord = x_coord
        self.y_coord = y_coord
        self.dist = 0
        self.updated = False

    def update_x_y_coord(self, x_coord, y_coord):
        self.x_coord = x_coord
        self.y_coord = y_coord
    
    def update_dist(self, dist):
        self.dist = dist
        self.updated = True
    
    def get_dist(self):
        self.updated = False
        return self.dist

    def __str__(self):
        return f"id = {self.id}, x_coord = {self.x_coord}, y_coord = {self.y_coord}"