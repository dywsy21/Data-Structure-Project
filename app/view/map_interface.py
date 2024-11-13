# coding: utf-8
from PyQt5.QtCore import Qt, QProcess, QPoint, QStringListModel
from PyQt5.QtGui import QMouseEvent, QPen, QPainterPath, QWheelEvent, QPixmap, QBrush, QColor
from PyQt5.QtWidgets import QWidget, QGraphicsView, QGraphicsScene, QVBoxLayout, QGraphicsPathItem, QGraphicsPixmapItem, QGraphicsEllipseItem, QHBoxLayout, QPushButton, QTextEdit, QSizePolicy, QGraphicsTextItem, QGraphicsItem, QCompleter, QGridLayout, QAction
from qfluentwidgets import * # type: ignore
import xml.etree.ElementTree as ET
from ..common.config import cfg, isWin11
from ..common.style_sheet import StyleSheet
from ..common.icon import Icon
import os
import subprocess
from ..common.signal_bus import signalBus


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
        self.gridSize = 100  # Define the size of each grid
        self.grids = {}  # Dictionary to store grid data
        self.initUI()
        signalBus.backendOutputReceived.connect(self.handle_backend_response)
        signalBus.backendErrorReceived.connect(self.handle_backend_error)
        self.pending_requests = {}
        self.request_id = 0

        # Emit location suggestions initially
        self.update_location_suggestions(self.get_all_available_names())

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

        # Left Layout for LineEdits and Algorithm Button
        self.leftLayout = QVBoxLayout()
        self.startLineEdit = SearchLineEdit(self.lowerWidget)
        self.endLineEdit = SearchLineEdit(self.lowerWidget)
        self.algorithmButton = SplitPushButton('Select Algorithm', self.lowerWidget)

        self.startLineEdit.setPlaceholderText("Select the start node")
        self.endLineEdit.setPlaceholderText("Select the end node")
        
        self.startLineEdit.setMaximumWidth(230)
        self.endLineEdit.setMaximumWidth(230)

        self.leftLayout.addWidget(self.algorithmButton)
        self.leftLayout.addWidget(self.startLineEdit)
        self.leftLayout.addWidget(self.endLineEdit)

        self.startCompleterModel = QStringListModel()
        self.startCompleter = QCompleter(self.startCompleterModel, self)
        self.startCompleter.setCaseSensitivity(Qt.CaseInsensitive)
        self.startCompleter.setMaxVisibleItems(10)
        self.startLineEdit.setCompleter(self.startCompleter)

        self.endCompleterModel = QStringListModel()
        self.endCompleter = QCompleter(self.endCompleterModel, self)
        self.endCompleter.setCaseSensitivity(Qt.CaseInsensitive)
        self.endCompleter.setMaxVisibleItems(10)
        self.endLineEdit.setCompleter(self.endCompleter)

        self.startLineEdit.searchButton.pressed.connect(self.on_start_line_edit_changed)
        self.endLineEdit.searchButton.pressed.connect(self.on_end_line_edit_changed)
        self.startLineEdit.returnPressed.connect(self.on_start_line_edit_changed)
        self.endLineEdit.returnPressed.connect(self.on_end_line_edit_changed)

        # Middle Layout for CheckBoxes
        self.middleLayout = QGridLayout()
        self.pedestrianCheckBox = CheckBox('Pedestrian', self.lowerWidget)
        self.ridingCheckBox = CheckBox('Riding', self.lowerWidget)
        self.drivingCheckBox = CheckBox('Driving', self.lowerWidget)
        self.pubTransportCheckBox = CheckBox('Public Transport', self.lowerWidget)

        self.pedestrianCheckBox.setChecked(self.pedestrain_enabled)
        self.ridingCheckBox.setChecked(self.riding_enabled)
        self.drivingCheckBox.setChecked(self.driving_enabled)
        self.pubTransportCheckBox.setChecked(self.pubTransport_enabled)

        self.pedestrianCheckBox.stateChanged.connect(self.on_checkbox_state_changed)
        self.ridingCheckBox.stateChanged.connect(self.on_checkbox_state_changed)
        self.drivingCheckBox.stateChanged.connect(self.on_checkbox_state_changed)
        self.pubTransportCheckBox.stateChanged.connect(self.on_checkbox_state_changed)

        self.middleLayout.addWidget(self.pedestrianCheckBox, 0, 0)
        self.middleLayout.addWidget(self.ridingCheckBox, 0, 1)
        self.middleLayout.addWidget(self.drivingCheckBox, 1, 0)
        self.middleLayout.addWidget(self.pubTransportCheckBox, 1, 1)

        # Right Layout for Buttons
        self.rightLayout = QVBoxLayout()
        self.showPathButton = PushButton('Show Shortest Path', self.lowerWidget)
        self.showPathButton.setMinimumSize(150, 40)  # Set minimum size
        self.showPathButton.clicked.connect(self.calculate_shortest_path)
        self.resetButton = PushButton('Reset', self.lowerWidget)
        self.resetButton.clicked.connect(self.reset)
        self.resetButton.setMinimumSize(150, 40)  # Set minimum size
        self.rightLayout.addWidget(self.showPathButton)
        self.rightLayout.addWidget(self.resetButton)

        # Add spacing between layouts
        self.lowerLayout.addLayout(self.leftLayout)
        self.lowerLayout.addSpacing(100)
        self.lowerLayout.addLayout(self.middleLayout)
        self.lowerLayout.addSpacing(100)
        self.lowerLayout.addLayout(self.rightLayout)

        self.mainLayout.addWidget(self.lowerWidget, 1)  # Stretch factor 1
        self.scene.setBackgroundBrush(QColor("#FFFFE0"))  # Set background to light yellow

        # Add algorithms to the algorithmButton
        self.addAlgorithmsToButton()

    def addAlgorithmsToButton(self):
        self.menu = RoundMenu(parent=self)
        algorithms = ["Dijkstra", "A*", "Bellman-Ford", "Floyd-Warshall"]
        for algorithm in algorithms:
            action = Action(algorithm, self)
            action.triggered.connect(lambda checked, alg=algorithm: self.setAlgorithm(alg))
            self.menu.addAction(action)
        self.algorithmButton.setFlyout(self.menu)

    def setAlgorithm(self, algorithm):
        self.selectedAlgorithm = algorithm
        self.algorithmButton.setText(f"Algorithm: {algorithm}")

    def reset(self):
        self.selectedNodes.clear()
        for item in self.pinItems:
            self.scene.removeItem(item)
        self.pinItems.clear()
        # self.scene.removeItem(self.currentPathItem)
        for path in self.drawnPaths:
            self.scene.removeItem(path)
        self.drawnPaths.clear()
        self.displayed_names.clear()  # Clear the set when resetting the map
        # clear the line edits
        self.startLineEdit.clear()
        self.endLineEdit.clear()
            
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
            signalBus.updateLocationSuggestions.emit(list(whitelist))
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
            node_name = self.get_node_name(nearest_node_id)
            if node_name:  # Ensure the node has a valid name
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
                if len(self.selectedNodes) == 1:
                    self.startLineEdit.setText(node_name)
                elif len(self.selectedNodes) == 2:
                    self.endLineEdit.setText(node_name)

    def get_node_name(self, node_id):
        way_id = self.node_to_way.get(node_id)
        if way_id:
            tags = self.ways[way_id]['tags']
            name = tags.get('name')
            if name:
                return name
            name_en = tags.get('name:en')
            if name_en:
                return name_en
        return None

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
        # remove the displayed path
        for path in self.drawnPaths:
            self.scene.removeItem(path)

    def find_nearest_node(self, point, return_distance=False):
        whitelist = self.get_whitelist()

        min_distance = None
        nearest_node_id = None
        for node_id, (lat, lon) in self.nodes.items():
            highway_type = self.get_highway_type_for_node(node_id)
            if highway_type not in whitelist or self.get_node_name(node_id) is None:
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

        # Send the search request to the backend via signalBus
        request = f"{node1_id} {node2_id}\n"
        signalBus.sendBackendRequest.emit(request)  # Ensure this line exists and is correct

    def handle_backend_response(self, output):
        lines = output.strip().split('\n')
        if "END" in lines:
            end_index = lines.index("END")
            path = lines[:end_index]
            if path and path[0] != '':
                # remove '\r' from each node id
                for i in range(len(path)):
                    path[i] = path[i].strip('\r')
                # print(path)
                self.display_path(path)
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
                pass
                # self.infoArea.setText("No path found between the selected nodes.")
        elif "NO PATH" in lines:
            pass
            # self.infoArea.setText("No path found between the selected nodes.")

        # Re-enable the button
        self.showPathButton.setEnabled(True)

    def handle_backend_error(self, error):
        InfoBar.error(
            title="Error",
            content=f"An error occurred while communicating with the backend: {error}",
            orient=Qt.Horizontal,
            isClosable=True,
            position=InfoBarPosition.BOTTOM_RIGHT,
            duration=2000,
            parent=self
        )
        self.showPathButton.setEnabled(True)

    def display_path(self, path):
        # # Remove existing path items
        # for path_item in self.drawnPaths:
        #     self.scene.removeItem(path_item)
        # self.drawnPaths.clear()
        
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
            path_item = QGraphicsPathItem(painter_path)
            pen = QPen(Qt.blue)
            pen.setWidthF(2 / self.view.scaleFactor)
            path_item.setPen(pen)
            self.scene.addItem(path_item)
            self.drawnPaths.append(path_item)

    def _connectSignalToSlot(self):
        """ connect signal to slot """
        cfg.themeChanged.connect(setTheme)
        signalBus.updateLocationSuggestions.connect(self.update_location_suggestions)

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

    def on_start_line_edit_changed(self):
        text = self.startLineEdit.text()
        self.update_pin_from_line_edit(text, self.startLineEdit)

    def on_end_line_edit_changed(self):
        text = self.endLineEdit.text()
        self.update_pin_from_line_edit(text, self.endLineEdit)

    def update_pin_from_line_edit(self, text, line_edit):
        nearest_node_id = self.find_nearest_node_by_name(text)
        if nearest_node_id:
            self.addPinAtNode(nearest_node_id)
            node_name = self.get_node_name(nearest_node_id)
            if line_edit == self.startLineEdit:
                if len(self.selectedNodes) == 0:
                    self.selectedNodes.append(nearest_node_id)
                else:
                    self.selectedNodes[0] = nearest_node_id
                # self.startLineEdit.setText(node_name)
            elif line_edit == self.endLineEdit:
                if len(self.selectedNodes) < 2:
                    self.selectedNodes.append(nearest_node_id)
                else:
                    self.selectedNodes[1] = nearest_node_id
                # self.endLineEdit.setText(node_name)

    def find_nearest_node_by_name(self, name):
        for node_id, (lat, lon) in self.nodes.items():
            way_id = self.node_to_way.get(node_id)
            if way_id:
                tags = self.ways[way_id]['tags']
                if name == tags.get('name') or name == tags.get('name:en'):
                    return node_id
        return None

    def update_location_suggestions(self, suggestions):
        self.startCompleterModel.setStringList(suggestions)
        self.endCompleterModel.setStringList(suggestions)
        self.startCompleter = QCompleter(self.startCompleterModel, self)
        self.endCompleter = QCompleter(self.endCompleterModel, self)
        self.startLineEdit.setCompleter(self.startCompleter)  # Set the completer again
        self.endLineEdit.setCompleter(self.endCompleter)      # Set the completer again

    def get_all_available_names(self):
        whitelist = self.get_whitelist()
        # Get all names from the nodes
        names = set()
        for node_id in self.nodes:
            if self.get_highway_type_for_node(node_id) in whitelist:
                name = self.get_node_name(node_id)
                if name:
                    names.add(name)
        return names

    def on_checkbox_state_changed(self, state):
        self.pedestrain_enabled = self.pedestrianCheckBox.isChecked()
        self.riding_enabled = self.ridingCheckBox.isChecked()
        self.driving_enabled = self.drivingCheckBox.isChecked()
        self.pubTransport_enabled = self.pubTransportCheckBox.isChecked()
        self.update_location_suggestions(self.get_all_available_names())
