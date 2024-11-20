# coding: utf-8
from PyQt5.QtCore import * # type: ignore
from PyQt5.QtGui import * # type: ignore
from PyQt5.QtWidgets import * # type: ignore
from qfluentwidgets import * # type: ignore
from PyQt5.QtWebEngineWidgets import * # type: ignore
from PyQt5.QtWebChannel import * # type: ignore
from ..common.config import *
from ..common.style_sheet import StyleSheet
from ..common.icon import Icon
import os
import subprocess
from ..common.signal_bus import signalBus
import math
from preprocess_map_html import map_html
from map_utils import *


class MapInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("mapInterface")
        self.pedestrain_enabled = True
        self.riding_enabled = True
        self.driving_enabled = True
        self.pubTransport_enabled = True
        self.selectedAlgorithm = None  # Add this line
        self.algorithmWarningShown = False  # Add this line
        self.on_start_line_edit_changed = pyqtBoundSignal()
        self.on_end_line_edit_changed = pyqtBoundSignal()
        self.on_checkbox_state_changed = pyqtBoundSignal()
        self.thread_pool = QThreadPool()
        signalBus.finishRenderingTile.connect(self.on_tiles_fetched)
        self.middlePoints = []  # Add this line
        self.max_distance = 0.01  # Max distance limit for removing nodes
        
        self.initUI()

    def initUI(self):
        self.mainLayout = QVBoxLayout(self)
        
        # Map View (75% height)
        self.browser = QWebEngineView()
        self.browser.loadFinished.connect(self.on_load_finished)

        # Expose Python object to JavaScript
        self.channel = QWebChannel()
        self.channel.registerObject('pyObj', self)
        self.browser.page().setWebChannel(self.channel)
        self.browser.page().javaScriptConsoleMessage = self.handleConsoleMessage  # Add this line
        
        # Load the generated HTML file
        self.browser.setUrl(QUrl(f"file:///{os.path.abspath(map_html_path).replace(os.sep, '/')}"))

        self.mainLayout.addWidget(self.browser, 4)

        # Clear the displayed names when initializing the map
        self.selectedNodes = []  # List to store selected node IDs
        self.pinItems = []       # List to store pin icons

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

        # self.startLineEdit.searchButton.pressed.connect(self.on_start_line_edit_changed)
        # self.endLineEdit.searchButton.pressed.connect(self.on_end_line_edit_changed)
        # self.startLineEdit.returnPressed.connect(self.on_start_line_edit_changed)
        # self.endLineEdit.returnPressed.connect(self.on_end_line_edit_changed)

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

        # self.pedestrianCheckBox.stateChanged.connect(self.on_checkbox_state_changed)
        # self.ridingCheckBox.stateChanged.connect(self.on_checkbox_state_changed)
        # self.drivingCheckBox.stateChanged.connect(self.on_checkbox_state_changed)
        # self.pubTransportCheckBox.stateChanged.connect(self.on_checkbox_state_changed)

        self.middleLayout.addWidget(self.pedestrianCheckBox, 0, 0)
        self.middleLayout.addWidget(self.ridingCheckBox, 0, 1)
        self.middleLayout.addWidget(self.drivingCheckBox, 1, 0)
        self.middleLayout.addWidget(self.pubTransportCheckBox, 1, 1)

        # Right Layout for Buttons
        self.rightLayout = QVBoxLayout()
        self.showPathButton = PushButton('Show Shortest Path', self.lowerWidget)
        self.showPathButton.setMinimumSize(150, 40)  # Set minimum size
        # self.showPathButton.clicked.connect(self.calculate_shortest_path)
        self.resetButton = PushButton('Reset', self.lowerWidget)
        # self.resetButton.clicked.connect(self.reset)
        self.resetButton.setMinimumSize(150, 40)  # Set minimum size
        self.rightLayout.addWidget(self.showPathButton)
        self.rightLayout.addWidget(self.resetButton)

        self.progressBar = ProgressBar(self.lowerWidget)
        self.progressBar.setMaximum(100)
        self.progressBar.setValue(0)
        self.progressBar.setVisible(False)

        self.rightLayout.addWidget(self.progressBar)

        # Add spacing between layouts
        self.lowerLayout.addLayout(self.leftLayout)
        self.lowerLayout.addSpacing(100)
        self.lowerLayout.addLayout(self.middleLayout)
        self.lowerLayout.addSpacing(100)
        self.lowerLayout.addLayout(self.rightLayout)

        self.mainLayout.addWidget(self.lowerWidget, 1)  # Stretch factor 1

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

    def handleConsoleMessage(self, level, message, lineNumber, sourceID):
        print(f"JS Console: {message} (Source: {sourceID}, Line: {lineNumber})")

    @pyqtSlot()
    def on_load_finished(self):
        # Initialize JavaScript code to access the Folium map
        self.browser.page().runJavaScript(f"""
            // Access the existing Folium map
            var map = window.{map_html.get_name()};

            // Ensure the map container has the correct ID
            document.getElementById('map').appendChild(map.getContainer());

            // Initialize markers array
            var markers = [];

            // Define custom signal class
            function Signal() {{
                this.listeners = [];
            }}
            Signal.prototype.connect = function(listener) {{
                this.listeners.push(listener);
            }};
            Signal.prototype.emit = function(data) {{
                this.listeners.forEach(function(listener) {{
                    listener(data);
                }});
            }};

            // Define mapProperties object
            var mapProperties = {{
                bounds: null,
                zoom: null,
                boundsChanged: new Signal(),
                zoomChanged: new Signal()
            }};

            // Connect to the PyQt WebChannel
            new QWebChannel(qt.webChannelTransport, function(channel) {{
                window.pyObj = channel.objects.pyObj;
                window.mapProperties = mapProperties;

                // Listen for property changes
                mapProperties.boundsChanged.connect(function(bounds) {{
                    pyObj.updateVisibleTiles(bounds, mapProperties.zoom);
                }});
                mapProperties.zoomChanged.connect(function(zoom) {{
                    pyObj.updateVisibleTiles(mapProperties.bounds, zoom);
                }});
            }});

            // Listen for moveend events
            map.on('moveend', function() {{
                var bounds = map.getBounds();
                var zoom = map.getZoom();
                mapProperties.bounds = JSON.stringify(bounds);
                mapProperties.zoom = zoom;
                mapProperties.boundsChanged.emit(mapProperties.bounds);
                mapProperties.zoomChanged.emit(mapProperties.zoom);
            }});

            // Define custom icons
            var yellowIcon = L.icon({{
                iconUrl: 'file:///E:/BaiduSyncdisk/Code%20Projects/PyQt%20Projects/Data%20Structure%20Project/app/resource/images/yellow_icon.svg',
                iconSize: [25, 41],
                iconAnchor: [12, 41],
                popupAnchor: [1, -34],
            }});

            console.log("Yellow icon created:", yellowIcon);

            // Listen for click events
            map.on('click', function(e) {{
                var lat = e.latlng.lat;
                var lng = e.latlng.lng;
                var isCtrlPressed = e.originalEvent.ctrlKey;
                if (isCtrlPressed) {{
                    // Add yellow pin for middle points
                    var marker = L.marker([lat, lng], {{icon: yellowIcon}}).addTo(map);
                    markers.push(marker);
                    console.log("Added yellow marker at:", lat, lng);
                    window.pyObj.addMiddlePoint(lat, lng);
                }} else {{
                    // Add default pin for selected nodes
                    var marker = L.marker([lat, lng]).addTo(map);
                    markers.push(marker);
                    console.log("Added default marker at:", lat, lng);
                    window.pyObj.addSelectedNode(lat, lng);
                }}
            }});

            map.on('contextmenu', function(e) {{
                var lat = e.latlng.lat;
                var lng = e.latlng.lng;
                window.pyObj.removeNearestNode(lat, lng);
                window.pyObj.removeNearestMiddlePoint(lat, lng);

                // Remove the nearest marker
                var nearestMarker = null;
                var minDistance = Infinity;
                markers.forEach(function(marker) {{
                    var markerLatLng = marker.getLatLng();
                    var distance = map.distance([lat, lng], markerLatLng);
                    if (distance < minDistance) {{
                        minDistance = distance;
                        nearestMarker = marker;
                    }}
                }});
                if (nearestMarker && minDistance <= {self.max_distance * 100000}) {{
                    map.removeLayer(nearestMarker);
                    markers = markers.filter(function(marker) {{
                        return marker !== nearestMarker;
                    }});
                    console.log("Removed marker at:", nearestMarker.getLatLng());
                }}
            }});
        """)

    @pyqtSlot(str, int)
    def updateVisibleTiles(self, bounds_json, zoom):
        bounds = json.loads(bounds_json)
        lat_min = bounds['_southWest']['lat']
        lon_min = bounds['_southWest']['lng']
        lat_max = bounds['_northEast']['lat']
        lon_max = bounds['_northEast']['lng']
        visible_tiles = calculate_visible_tiles([(lat_min, lon_min), (lat_max, lon_max)], zoom)
        
        # Start a worker for each tile to fetch nodes from the database
        for tile in visible_tiles:
            print(zoom, tile[0], tile[1])
            signalBus.renderTile.emit(zoom, tile[0], tile[1])

    def on_tiles_fetched(self, z, x, y):
        custom_tile_layer_js = """
            var custom_tile_layer = L.tileLayer(
                "renderer/cache/{z}/{x}/{y}.png",
                {{"attribution": "Custom Tiles", "maxZoom": 19}}
            );
            custom_tile_layer.addTo(map);
        """

        self.browser.page().runJavaScript(custom_tile_layer_js)

    @pyqtSlot(float, float)
    def addSelectedNode(self, lat, lng):
        self.selectedNodes.append((lat, lng))

    @pyqtSlot(float, float)
    def addMiddlePoint(self, lat, lng):
        self.middlePoints.append((lat, lng))

    @pyqtSlot(float, float)
    def removeNearestNode(self, lat, lng):
        def distance(node):
            return math.sqrt((node[0] - lat) ** 2 + (node[1] - lng) ** 2)

        if self.selectedNodes:
            nearest_node = min(self.selectedNodes, key=distance)
            if distance(nearest_node) <= self.max_distance:
                self.selectedNodes.remove(nearest_node)
                print(f"Removed node at: {nearest_node}")

    @pyqtSlot(float, float)
    def removeNearestMiddlePoint(self, lat, lng):
        def distance(point):
            return math.sqrt((point[0] - lat) ** 2 + (point[1] - lng) ** 2)

        if self.middlePoints:
            nearest_point = min(self.middlePoints, key=distance)
            if distance(nearest_point) <= self.max_distance:
                self.middlePoints.remove(nearest_point)
                print(f"Removed middle point at: {nearest_point}")
