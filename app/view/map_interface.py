# coding: utf-8
from PyQt5.QtCore import Qt, QProcess, QPoint
from PyQt5.QtGui import QMouseEvent, QPen, QPainterPath, QWheelEvent, QPixmap, QBrush, QColor
from PyQt5.QtWidgets import QWidget, QGraphicsView, QGraphicsScene, QVBoxLayout, QGraphicsPathItem, QGraphicsPixmapItem, QGraphicsEllipseItem, QHBoxLayout, QPushButton, QTextEdit, QSizePolicy
from qfluentwidgets import * # type: ignore
import xml.etree.ElementTree as ET
from ..common.config import cfg, isWin11
from ..common.style_sheet import StyleSheet
from ..common.icon import Icon
import os


class MapGraphicsView(QGraphicsView):
    def __init__(self, *args, **kwargs):
        super(MapGraphicsView, self).__init__(*args, **kwargs)
        self.setDragMode(QGraphicsView.NoDrag)
        self._pan = False
        self._panStartX = None
        self._panStartY = None
        self.scaleFactor = 1.0  # Keep track of the current scale factor
        StyleSheet.MAP_INTERFACE.apply(self)

    def wheelEvent(self, event: QWheelEvent):
        if event.modifiers() == Qt.ControlModifier:
            # Zoom factor
            zoomInFactor = 1.15
            zoomOutFactor = 1 / zoomInFactor

            if event.angleDelta().y() > 0:
                zoomFactor = zoomInFactor
            else:
                zoomFactor = zoomOutFactor

            self.scale(zoomFactor, zoomFactor)
            self.scaleFactor *= zoomFactor  # Update the scale factor

            # Update line thickness and icon scales based on new scale factor
            self.parentWidget().updateLineThickness(self.scaleFactor) # type: ignore
        else:
            super(MapGraphicsView, self).wheelEvent(event)

    def mousePressEvent(self, event: QMouseEvent):
        if event.button() == Qt.LeftButton:
            # Pass the event to parent to handle node selection
            scene_pos = self.mapToScene(event.pos())
            self.parentWidget().handleMapClick(scene_pos) # type: ignore
        elif event.button() == Qt.RightButton:
            # Pass the event to parent to handle node removal
            scene_pos = self.mapToScene(event.pos())
            self.parentWidget().handleMapRightClick(scene_pos) # type: ignore
        else:
            super(MapGraphicsView, self).mousePressEvent(event)

    def mouseMoveEvent(self, event: QMouseEvent):
        if self._pan:
            deltaX = self._panStartX - event.x() # type: ignore
            deltaY = self._panStartY - event.y() # type: ignore
            self._panStartX = event.x()
            self._panStartY = event.y()
            self.horizontalScrollBar().setValue(self.horizontalScrollBar().value() + deltaX)
            self.verticalScrollBar().setValue(self.verticalScrollBar().value() + deltaY)
        else:
            super(MapGraphicsView, self).mouseMoveEvent(event)

    def mouseReleaseEvent(self, event: QMouseEvent):
        if event.button() == Qt.LeftButton and self._pan:
            self._pan = False
            self.setCursor(Qt.ArrowCursor)
        else:
            super(MapGraphicsView, self).mouseReleaseEvent(event)

class MapInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.tag_colors = self.load_tag_colors()
        self.setObjectName("MapInterface")
        self.points = []
        self.nodes = {}  # Dictionary to store node data
        self.ways = []   # List to store way data
        self.drawnPaths = []  # List to store drawn paths
        self.enclosed_tags = {'building', 'park', 'garden', 'leisure', 'landuse', 'natural', 'historic'}
        self.initUI()

    def initUI(self):
        self.mainLayout = QVBoxLayout(self)
        
        # Map View (75% height)
        self.scene = QGraphicsScene(self)
        self.view = MapGraphicsView(self.scene, self)
        self.view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.mainLayout.addWidget(self.view, 3)  # Stretch factor 3

        self.pathItems = []  # Store references to path items
        self.load_osm_data_and_draw_out(r'E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\backend\data\map_large')
        self.selectedNodes = []  # List to store selected node IDs
        self.pinItems = []       # List to store pin icons
        self.iconPixmap = QPixmap(r':app/images/location icon.png')  # Load the locating icon

        # Lower Area (25% height)
        self.lowerWidget = QWidget(self)
        self.lowerLayout = QHBoxLayout(self.lowerWidget)

        self.lowerRightWidget = QVBoxLayout(self.lowerWidget)
        
        # Info Area (Left)
        self.infoArea = TextEdit(self.lowerWidget)
        self.infoArea.setReadOnly(True)
        self.lowerLayout.addWidget(self.infoArea)

        self.lowerLayout.addLayout(self.lowerRightWidget)

        # Show Shortest Path Button (Right)
        self.showPathButton = PushButton('Show Shortest Path', self.lowerWidget)
        self.showPathButton.setMinimumSize(150, 40)  # Set minimum size

        self.showPathButton.clicked.connect(self.calculate_shortest_path)
        self.lowerRightWidget.addWidget(self.showPathButton)

        self.resetButton = PushButton('Reset', self.lowerWidget)
        self.resetButton.clicked.connect(self.reset)
        self.resetButton.setMinimumSize(150, 40)  # Set minimum size

        self.lowerRightWidget.addWidget(self.resetButton)

        self.mainLayout.addWidget(self.lowerWidget, 1)  # Stretch factor 1
        self.scene.setBackgroundBrush(QColor("#FFFFE0"))  # Set background to light yellow
    
    def reset(self):
        self.selectedNodes.clear()
        for item in self.pinItems:
            self.scene.removeItem(item)
        self.pinItems.clear()
        # self.scene.removeItem(self.currentPathItem)
        self.infoArea.clear()
        for path in self.drawnPaths:
            self.scene.removeItem(path)
        self.drawnPaths.clear()
            

    def load_osm_data_and_draw_out(self, osm_file):
        tree = ET.parse(osm_file)
        root = tree.getroot()

        for node in root.findall('node'):
            node_id = node.get('id')
            lat = float(node.get('lat')) # type: ignore
            lon = float(node.get('lon')) # type: ignore
            self.nodes[node_id] = (lat, lon)

        # s = set()
        for way in root.findall('way'):
            way_id = way.get('id')
            node_refs = [nd.get('ref') for nd in way.findall('nd')]
            tags = {tag.get('k'): tag.get('v') for tag in way.findall('tag')}
            # if tags.keys():
            #     s.add(list(tags.keys())[0])
            self.ways.append({'id': way_id, 'nodes': node_refs, 'tags': tags})
        # with open(r'e:/BaiduSyncdisk/Code Projects/PyQt Projects/Data Structure Project/tags.txt', 'w') as f:
        #     for item in s:
        #         if ":" not in item:
        #             f.write(item + '\n')
        self.draw_map()

    def draw_map(self):
        # Compute bounding box of all nodes
        lats = [lat for lat, lon in self.nodes.values()]
        lons = [lon for lat, lon in self.nodes.values()]
        min_lat, max_lat = min(lats), max(lats)
        min_lon, max_lon = min(lons), max(lons)

        # Set scene dimensions
        scene_width = 800
        scene_height = 600
        self.scene.setSceneRect(0, 0, scene_width, scene_height)

        # Map lat/lon to scene coordinates
        def map_to_scene(lat, lon):
            x = (lon - min_lon) / (max_lon - min_lon) * scene_width
            y = (lat - min_lat) / (max_lat - min_lat) * scene_height
            y = scene_height - y  # Invert y-axis if necessary
            return x, y

        self.min_lat, self.max_lat = min_lat, max_lat
        self.min_lon, self.max_lon = min_lon, max_lon
        self.scene_width = scene_width
        self.scene_height = scene_height

        self.map_to_scene = map_to_scene  # Assign to instance for reuse

        for way in self.ways:
            node_refs = way['nodes']
            first_point = True
            path = QPainterPath()
            points = []
            for node_id in node_refs:
                if node_id in self.nodes:
                    lat, lon = self.nodes[node_id]
                    x, y = map_to_scene(lat, lon)
                    points.append((x, y))
                    if first_point:
                        path.moveTo(x, y)
                        first_point = False
                    else:
                        path.lineTo(x, y)
            if not first_point:
                tags = way['tags']
                color = self.tag_colors.get(next(iter(tags), ''), QColor("#ADD8E6"))  # Changed from Qt.black to light blue
                
                # Check if the way is closed and has an enclosed tag
                if any(tag in self.enclosed_tags for tag in tags) and node_refs[0] == node_refs[-1]:
                    # Draw filled polygon for enclosed areas
                    polygon = QPainterPath()
                    polygon.moveTo(*points[0])
                    for point in points[1:]:
                        polygon.lineTo(*point)
                    polygon.closeSubpath()
                    polygonItem = QGraphicsPathItem(polygon)
                    pen = QPen(color)
                    pen.setWidthF(self.getPenWidth())
                    polygonItem.setPen(pen)
                    polygonItem.setBrush(QColor(color))
                    self.scene.addItem(polygonItem)
                    self.pathItems.append(polygonItem)
                else:
                    pen = QPen(color)
                    pen.setWidthF(self.getPenWidth())
                    pathItem = QGraphicsPathItem(path)
                    pathItem.setPen(pen)
                    self.scene.addItem(pathItem)
                    self.pathItems.append(pathItem)

    def getPenWidth(self):
        # Calculate pen width based on the current scale factor
        pen_width = 1.0 / self.view.scaleFactor
        pen_width = max(min(pen_width, 3.0), 0.1)  # Limit pen width between 0.1 and 3.0
        return pen_width

    def updateLineThickness(self, scaleFactor):
        # Update pen widths of existing path items
        pen_width = self.getPenWidth()
        for item in self.pathItems:
            pen = item.pen()
            pen.setWidthF(pen_width)
            item.setPen(pen)
        # Also update the icon scales
        self.updateIconScale()

    def handleMapClick(self, scene_pos):
        nearest_node_id = self.find_nearest_node(scene_pos)
        if nearest_node_id:
            # Store up to two selected nodes
            if len(self.selectedNodes) >= 2:
                # Remove previous pins
                for item in self.pinItems:
                    self.scene.removeItem(item)
                self.pinItems.clear()
                self.selectedNodes.clear()

            self.addPinAtNode(nearest_node_id)
            self.selectedNodes.append(nearest_node_id)

    def handleMapRightClick(self, scene_pos):
        # Find nearest node to right-click position
        for node_id in self.selectedNodes:
            # Get the node's position
            lat, lon = self.nodes[node_id]
            x, y = self.map_to_scene(lat, lon)
            # Calculate distance between click position and node position
            distance = (scene_pos.x() - x)**2 + (scene_pos.y() - y)**2
            if distance is not None and distance < 500:
                self.removeNodeFromSelectedNodes(node_id)

    def find_nearest_node(self, point, return_distance=False):
        min_distance = None
        nearest_node_id = None
        for node_id, (lat, lon) in self.nodes.items():
            x, y = self.map_to_scene(lat, lon)
            distance = (x - point.x())**2 + (y - point.y())**2
            if min_distance is None or distance < min_distance:
                min_distance = distance
                nearest_node_id = node_id
        if return_distance:
            return nearest_node_id, min_distance
        else:
            return nearest_node_id

    def removeNodeFromSelectedNodes(self, node_id):
        if node_id in self.selectedNodes:
            index = self.selectedNodes.index(node_id)
            self.selectedNodes.pop(index)
            pin_item = self.pinItems.pop(index)
            self.scene.removeItem(pin_item)

    def addPinAtNode(self, node_id):
        lat, lon = self.nodes[node_id]
        x, y = self.map_to_scene(lat, lon)
        # Create a QGraphicsPixmapItem to represent the pin
        pin_item = QGraphicsPixmapItem(self.iconPixmap)
        pin_item.setOffset(-self.iconPixmap.width() / 2, -self.iconPixmap.height()) # -self.iconPixmap.height() / 2
        pin_item.setPos(x, y)
        pin_item.setScale(self.getIconScale())
        self.scene.addItem(pin_item)
        self.pinItems.append(pin_item)

    def updateIconScale(self):
        # Update the scale of the pin icons based on the current zoom level
        icon_scale = self.getIconScale()
        for pin_item in self.pinItems:
            pin_item.setScale(icon_scale)

    def getIconScale(self):
        # Calculate icon scale factor based on the current scale factor
        base_scale = 0.02  # Adjust as needed
        scale = base_scale / self.view.scaleFactor
        return scale

    def calculate_shortest_path(self):
        if len(self.selectedNodes) < 2:
            self.infoArea.setText("Please select two nodes to calculate the shortest path.")
            return
        
        node1_id, node2_id = self.selectedNodes
        # Call the backend or implement the pathfinding algorithm
        path = self.find_shortest_path(node1_id, node2_id)
        
        if path:
            self.display_path(path)
            self.infoArea.setText(f"Shortest path from {node1_id} to {node2_id}:\n" + "\n".join(path))
        else:
            self.infoArea.setText("No path found between the selected nodes.")

    def find_shortest_path(self, node1_id, node2_id):
        # Implement your pathfinding algorithm or call the backend
        # For demonstration, return a list of node IDs forming a path
        # Example:
        return [node1_id, 'node_mid', node2_id]

    def display_path(self, path):
        # # Remove existing path items
        # if hasattr(self, 'currentPathItem'):
        #     self.scene.removeItem(self.currentPathItem)
        
        # Create a new path
        path_points = []
        for node_id in path:
            if node_id in self.nodes:
                lat, lon = self.nodes[node_id]
                x, y = self.map_to_scene(lat, lon)
                path_points.append((x, y))
        
        if path_points:
            painter_path = QPainterPath()
            painter_path.moveTo(*path_points[0])
            for point in path_points[1:]:
                painter_path.lineTo(*point)
            self.currentPathItem = QGraphicsPathItem(painter_path)
            pen = QPen(Qt.blue)
            pen.setWidthF(2 / self.view.scaleFactor)
            self.currentPathItem.setPen(pen)
            self.scene.addItem(self.currentPathItem)
            self.drawnPaths.append(self.currentPathItem)

    def _connectSignalToSlot(self):
        """ connect signal to slot """
        cfg.themeChanged.connect(setTheme)

    def load_tag_colors(self):
        color_mapping = {}
        tags_file = r'e:/BaiduSyncdisk/Code Projects/PyQt Projects/Data Structure Project/tags.txt'
        if os.path.exists(tags_file):
            with open(tags_file, 'r') as f:
                for line in f:
                    if line.strip():
                        tag, color = line.strip().split(':')
                        color_mapping[tag] = QColor(color)
        return color_mapping
