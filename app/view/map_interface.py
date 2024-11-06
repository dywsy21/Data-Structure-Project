# coding: utf-8
from PyQt5.QtCore import Qt, QProcess, QPoint
from PyQt5.QtGui import QMouseEvent, QPen, QPainterPath, QWheelEvent, QPixmap, QBrush, QColor
from PyQt5.QtWidgets import QWidget, QGraphicsView, QGraphicsScene, QVBoxLayout, QGraphicsPathItem, QGraphicsPixmapItem, QGraphicsEllipseItem, QHBoxLayout, QPushButton, QTextEdit, QSizePolicy, QGraphicsTextItem, QGraphicsItem
from qfluentwidgets import * # type: ignore
import xml.etree.ElementTree as ET
from ..common.config import cfg, isWin11
from ..common.style_sheet import StyleSheet
from ..common.icon import Icon
import os
import subprocess


class MapGraphicsView(QGraphicsView):
    def __init__(self, *args, **kwargs):
        super(MapGraphicsView, self).__init__(*args, **kwargs)
        self.setDragMode(QGraphicsView.NoDrag)
        self._pan = False
        self._panStartX = None
        self._panStartY = None
        self.scaleFactor = 1  # Keep track of the current scale factor
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
        self.ways = {}   # Change self.ways to a dictionary
        self.drawnPaths = []  # List to store drawn paths
        self.enclosed_tags = {'building', 'park', 'garden', 'leisure', 'landuse', 'natural', 'historic'}
        self.pedestrain_enabled = True
        self.riding_enabled = True
        self.driving_enabled = True
        self.pubTransport_enabled = True
        self.node_to_way = {}
        self.is_highway_node = {}
        self.process = None  # Add this line to initialize the process attribute
        self.textItems = []           # List to store text items
        self.nameTagThreshold = 16    # Define the minimum scaleFactor to display name tags
        self.displayed_names = set()  # Set to track names already displayed
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
        # Clear the displayed names when initializing the map
        self.displayed_names.clear()
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
        self.displayed_names.clear()  # Clear the set when resetting the map
            
    def get_whitelist(self):
        # Define mapping between modes and highway types
        modes_to_highway = {
            'pedestrain': {'pedestrian', 'footway', 'steps', 'path', 'living_street'},
            'riding': {'cycleway', 'path', 'track'},
            'driving': {'motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'service',
                       'motorway_link', 'trunk_link', 'primary_link', 'secondary_link', 'residential'},
            'pubTransport': {'bus_stop', 'motorway_junction', 'traffic_signals', 'crossing'}
        }

        whitelist = set()
        if self.pedestrain_enabled:
            whitelist.update(modes_to_highway['pedestrain'])
        if self.riding_enabled:
            whitelist.update(modes_to_highway['riding'])
        if self.driving_enabled:
            whitelist.update(modes_to_highway['driving'])
        if self.pubTransport_enabled:
            whitelist.update(modes_to_highway['pubTransport'])

        # Read the valid highway types from output.txt
        output_file_path = r'E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\auxilary\output.txt'
        try:
            with open(output_file_path, 'r') as f:
                valid_highways = {line.strip() for line in f if line.strip()}
        except FileNotFoundError:
            return whitelist

        # Filter whitelist to include only valid highways
        whitelist = whitelist.intersection(valid_highways)

        return whitelist

    def load_osm_data_and_draw_out(self, osm_file):
        tree = ET.parse(osm_file)
        root = tree.getroot()

        for node in root.findall('node'):
            node_id = node.get('id')
            lat = float(node.get('lat')) # type: ignore
            lon = float(node.get('lon')) # type: ignore
            self.nodes[node_id] = (lat, lon)

        for way in root.findall('way'):
            way_id = way.get('id')
            node_refs = [nd.get('ref') for nd in way.findall('nd')]
            tags = {tag.get('k'): tag.get('v') for tag in way.findall('tag')}
            # Store way information in the dictionary using way_id as the key
            self.ways[way_id] = {'nodes': node_refs, 'tags': tags}

            # Update node_to_way mapping
            for node_id in node_refs:
                self.node_to_way[node_id] = way_id

            # Initialize is_highway_node
            if 'highway' in tags:
                for node_id in node_refs:
                    self.is_highway_node[node_id] = True

        # Clear the set before loading new data
        self.displayed_names.clear()
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

        for way in self.ways.values():
            node_refs = way['nodes']
            first_point = True
            path = QPainterPath()
            points = []
            for node_id in node_refs:
                if node_id in self.nodes:
                    lat, lon = self.nodes[node_id]
                    x, y = self.map_to_scene(lat, lon)
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

                # After drawing the way, check for 'name' tag
                if 'name' in tags:
                    name = tags['name']
                    # Check if the name has already been displayed
                    if name not in self.displayed_names:
                        # Calculate the centroid of the points to position the label
                        x_coords = [point[0] for point in points]
                        y_coords = [point[1] for point in points]
                        centroid_x = sum(x_coords) / len(x_coords)
                        centroid_y = sum(y_coords) / len(y_coords)

                        # Create a QGraphicsTextItem to display the name
                        text_item = QGraphicsTextItem(name)
                        text_item.setDefaultTextColor(QColor("#768ef9"))  # Changed text color to #768ef9
                        font = text_item.font()
                        font.setFamily("Microsoft YaHei")  # Changed font to Microsoft YaHei for better aesthetics with Chinese characters
                        font.setPointSizeF(self.getFontSize())  # Maintain dynamic font sizing
                        text_item.setFont(font)
                        text_item.setPos(centroid_x, centroid_y)
                        text_item.setZValue(1)  # Ensure the text is on top
                        text_item.setFlag(QGraphicsItem.ItemIgnoresTransformations, True)
                        
                        # Set visibility based on current scaleFactor
                        if self.view.scaleFactor >= self.nameTagThreshold:
                            text_item.setVisible(True)
                        else:
                            text_item.setVisible(False)
                        
                        self.scene.addItem(text_item)
                        self.textItems.append(text_item)          # Store reference for updates
                        self.displayed_names.add(name)            # Add name to the set to prevent duplication

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
        # Update text item font sizes
        self.updateTextItemFontSizes()
        # Update text item visibility based on zoom threshold
        self.updateTextItemVisibility()

    def updateTextItemFontSizes(self):
        for text_item in self.textItems:
            font = text_item.font()
            font.setPointSizeF(self.getFontSize())
            text_item.setFont(font)

    def updateTextItemVisibility(self):
        # print(self.view.scaleFactor)
        if self.view.scaleFactor >= self.nameTagThreshold:
            for text_item in self.textItems:
                text_item.setVisible(True)
        else:
            for text_item in self.textItems:
                text_item.setVisible(False)

    def getFontSize(self):
        # Calculate font size based on current scale factor
        base_font_size = 0.4  # Base font size at initial scaleFactor of 2.5
        return base_font_size * (self.view.scaleFactor / 2.5)

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
                # Remove previously drawn path
                for path in self.drawnPaths:
                    self.scene.removeItem(path)

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
        whitelist = self.get_whitelist()

        min_distance = None
        nearest_node_id = None
        for node_id, (lat, lon) in self.nodes.items():
            highway_type = self.get_highway_type_for_node(node_id)
            if highway_type not in whitelist:
                continue

            x, y = self.map_to_scene(lat, lon)
            distance = (x - point.x())**2 + (y - point.y())**2
            if min_distance is None or distance < min_distance:
                min_distance = distance
                nearest_node_id = node_id

        if return_distance:
            return nearest_node_id, min_distance
        else:
            return nearest_node_id

    def get_highway_type_for_node(self, node_id):
        # use is_highway_node to determine if a node is a highway node
        if node_id in self.is_highway_node:
            # use node_to_way to get the way id, and then get the tags of the way
            way_id = self.node_to_way[node_id]
            tags = self.ways[way_id]['tags']
            return tags.get('highway', '')
    
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
            InfoBar.error(
                title="Error",
                content="Please select two nodes to calculate the shortest path.",
                orient=Qt.Horizontal,
                isClosable=True,
                position=InfoBarPosition.BOTTOM_RIGHT,
                duration=2000,
                parent=self
            )
            return

        node1_id, node2_id = self.selectedNodes

        # Disable the button to prevent multiple clicks
        self.showPathButton.setEnabled(False)
        self.infoArea.setText("Calculating shortest path...")

        # Start the asynchronous process
        self.find_shortest_path(node1_id, node2_id)

    def find_shortest_path(self, node1_id, node2_id):
        # Create a QProcess instance if not already created
        if self.process is None:
            self.process = QProcess(self)
            self.process.finished.connect(self.on_process_finished)
            self.process.errorOccurred.connect(self.on_process_error)

        # Build the command and arguments
        command = 'build/backend.exe'
        arguments = [node1_id, node2_id]

        # Start the process
        print(f"Starting process: {command} {node1_id} {node2_id}")
        self.process.start(command, arguments)

    def on_process_finished(self, exitCode, exitStatus):
        # Read the full standard output
        output = self.process.readAllStandardOutput().data().decode() # type: ignore
        # Parse the output into a path
        path = output.strip().split('\r\n')

        if path and path[0] != '':
            self.display_path(path)
            self.infoArea.setText(
                f"Shortest path from {self.selectedNodes[0]} to {self.selectedNodes[1]}:\n" + "\n".join(path)
            )
            InfoBar.success(
                title="Success",
                content="Shortest path found successfully!",
                orient=Qt.Horizontal,
                isClosable=True,
                position=InfoBarPosition.BOTTOM_RIGHT,
                duration=2000,
                parent=self
            )
        else:
            self.infoArea.setText("No path found between the selected nodes.")

        # Re-enable the button
        self.showPathButton.setEnabled(True)

    def on_process_error(self, error):
        error_message = self.process.errorString() # type: ignore
        InfoBar.error(
            title="Error",
            content=f"An error occurred while calculating the shortest path: {error_message}",
            orient=Qt.Horizontal,
            isClosable=True,
            position=InfoBarPosition.BOTTOM_RIGHT,
            duration=2000,
            parent=self
        )
        self.showPathButton.setEnabled(True)

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
