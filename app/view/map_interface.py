# coding: utf-8
from multiprocessing import process
from PyQt5.QtCore import * # type: ignore
from PyQt5.QtGui import * # type: ignore
from PyQt5.QtWidgets import * # type: ignore
from inquirer import Checkbox
from numpy import signbit
from qfluentwidgets import * # type: ignore
from PyQt5.QtWebEngineWidgets import * # type: ignore
from PyQt5.QtWebChannel import * # type: ignore
from ..common.config import *
from ..common.style_sheet import StyleSheet
from ..common.icon import Icon
import os, shutil
import subprocess
from ..common.signal_bus import signalBus
import math
from map_utils import *
from ..common.config import *
import concurrent.futures

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
        self.custom_tile_layer_ids = []  # Store layer IDs instead of layer objects
        self.executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)  # Add this line
        
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

        # Add a SplitPushButton to toggle layer visibility
        self.layerToggleButton = SplitPushButton("Select Map Layer", self.lowerWidget)
        self.addLayersToButton()
        self.layerToggleButton.clicked.connect(self.onLayerToggleButtonClicked)
        self.rightLayout.addWidget(self.layerToggleButton)

        # Add spacing between layouts
        self.lowerLayout.addLayout(self.leftLayout)
        self.lowerLayout.addSpacing(100)
        self.lowerLayout.addLayout(self.middleLayout)
        self.lowerLayout.addSpacing(100)
        self.lowerLayout.addLayout(self.rightLayout)

        self.mainLayout.addWidget(self.lowerWidget, 1)  # Stretch factor 1

        # Add algorithms to the algorithmButton
        self.addAlgorithmsToButton()

        # Add a button to control the base map layer visibility
        self.addBaseLayerControlButton()

    def addAlgorithmsToButton(self):
            self.menu = RoundMenu(parent=self)
            algorithms = ["Dijkstra", "A*", "Bellman-Ford", "Floyd-Warshall"]
            for algorithm in algorithms:
                action = Action(algorithm, self)
                action.triggered.connect(lambda checked, alg=algorithm: self.setAlgorithm(alg))
                self.menu.addAction(action)
            self.algorithmButton.setFlyout(self.menu)

    def addLayersToButton(self):
        self.layerMenu = RoundMenu(parent=self)
        layers = [("Default Map", "default"), ("Custom Rendered Map", "custom"), ("Custom Rendered Map (Sparse)", "custom_sparse")]
        for layer_name, layer_type in layers:
            action = Action(layer_name, self)
            action.triggered.connect(lambda checked, lt=layer_type: self.setLayer(lt))
            self.layerMenu.addAction(action)
        self.layerToggleButton.setFlyout(self.layerMenu)

    def setAlgorithm(self, algorithm):
        self.selectedAlgorithm = algorithm
        self.algorithmButton.setText(f"Algorithm: {algorithm}")

    def setLayer(self, layerType):
        self.layerToggleButton.setText(f"Layer: {layerType.replace('_', ' ').title()}")
        self.toggleLayerVisibility(layerType)
        self.currentLayerType = layerType  # Store the current layer type

    @pyqtSlot(str)
    def toggleLayerVisibility(self, layerType):
        self.browser.page().runJavaScript(f"toggleLayerVisibility('{layerType}');")

    def onLayerToggleButtonClicked(self):
        current_text = self.layerToggleButton.text()
        if "Default Map" in current_text:
            self.toggleLayerVisibility("default")
        elif "Custom Rendered Map" in current_text:
            self.toggleLayerVisibility("custom")
        elif "Custom Rendered Map (Sparse)" in current_text:
            self.toggleLayerVisibility("custom_sparse")

    @pyqtSlot(int, str, int, str)
    def handleConsoleMessage(self, level, message, lineNumber, sourceID):
        level_str = {
            0: "DEBUG",
            1: "INFO",
            2: "WARNING",
            3: "ERROR"
        }.get(level, "UNKNOWN")
        
        print(f"[JS {level_str}] {message}")
        if sourceID or lineNumber:
            print(f"Source: {sourceID}, Line: {lineNumber}")

    @pyqtSlot()
    def on_load_finished(self):
        # Initialize JavaScript code to access the Folium map
        self.browser.page().runJavaScript("""
            // Redirect console messages to Python
            var originalConsole = window.console;
            window.console = {
                log: function() {
                    try {
                        var args = Array.prototype.slice.call(arguments);
                        var message = args.map(String).join(' ');
                        window.pyObj.handleConsoleMessage(1, message, 0, '');
                        originalConsole.log.apply(originalConsole, args);
                    } catch(e) {
                        originalConsole.error('Error in console.log override:', e);
                    }
                },
                warn: function() {
                    try {
                        var args = Array.prototype.slice.call(arguments);
                        var message = args.map(String).join(' ');
                        window.pyObj.handleConsoleMessage(2, message, 0, '');
                        originalConsole.warn.apply(originalConsole, args);
                    } catch(e) {
                        originalConsole.error('Error in console.warn override:', e);
                    }
                },
                error: function() {
                    try {
                        var args = Array.prototype.slice.call(arguments);
                        var message = args.map(String).join(' ');
                        window.pyObj.handleConsoleMessage(3, message, 0, '');
                        originalConsole.error.apply(originalConsole, args);
                    } catch(e) {
                        originalConsole.error('Error in console.error override:', e);
                    }
                }
            };
        """)

        
        self.browser.page().runJavaScript(f"""
            // Access the global map instance
            var map = window.map;

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
                    pyObj.updateVisibleTiles(JSON.stringify(bounds), mapProperties.zoom);
                }});
                mapProperties.zoomChanged.connect(function(zoom) {{
                    pyObj.updateVisibleTiles(JSON.stringify(mapProperties.bounds), zoom);
                }});
            }});

            // Listen for moveend events
            map.on('moveend', function() {{
                try {{
                    var bounds = map.getBounds();
                    // Extract only the necessary data from bounds
                    var boundsData = {{
                        _southWest: {{
                            lat: bounds.getSouth(),
                            lng: bounds.getWest()
                        }},
                        _northEast: {{
                            lat: bounds.getNorth(),
                            lng: bounds.getEast()
                        }}
                    }};
                    var zoom = map.getZoom();
                    
                    // Store values separately to avoid circular references
                    mapProperties.bounds = boundsData;
                    mapProperties.zoom = zoom;
                    
                    // Emit events with simple data
                    mapProperties.boundsChanged.emit(boundsData);
                    mapProperties.zoomChanged.emit(zoom);
                }} catch(e) {{
                    console.error('Error in moveend handler:', e);
                }}
            }});

            // Define custom icons
            var yellowIcon = L.icon({{
                iconUrl: 'file:///E:/BaiduSyncdisk/Code%20Projects/PyQt%20Projects/Data%20Structure%20Project/app/resource/images/yellow_icon.svg',
                iconSize: [25, 41],
                iconAnchor: [12, 41],
                popupAnchor: [1, -34],
            }});

            console.log("Yellow icon created:", yellowIcon);

            // Initialize custom tile layer group
            window.customTileLayerGroup = L.layerGroup().addTo(map);

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
                    if (markers.length >= 2) {{
                        markers.forEach(function(marker) {{
                            map.removeLayer(marker);
                        }});
                        markers = [];
                        window.pyObj.clearSelectedNodes();
                    }}
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

            function refreshCustomLayerGroup() {{
                window.customLayerGroup.eachLayer(function (layer) {{
                    Object.values(layer._tiles).forEach(function(tile) {{
                        if (true) {{
                            var img = tile.el;
                            var src = img.src;
                            img.src = ''; // Clear the src to force reload
                            img.src = src; // Set the src back to reload the tile
                        }}
                    }});
                }});
            }}

            setInterval(refreshCustomLayerGroup, 1000); // Refresh every second

        """)

    @pyqtSlot()
    def clearSelectedNodes(self):
        self.selectedNodes.clear()
        print("Cleared all selected nodes")

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
            self.begin_rendering_tile(zoom, tile[0], tile[1])
    
    def begin_rendering_tile(self, z, x, y):
        if self.currentLayerType == "custom" and z <= 13:
            renderer_path = "renderer/target/release/renderer.exe"
        else:
            renderer_path = "renderer_sparse/target/release/renderer.exe"

        if os.path.exists(f"{renderer_path}/cache/{z}/{x}_{y}.png"):
            if self.currentLayerType == "custom" and z > 13:
                # copy the tile to the custom layer
                os.makedirs(f"renderer/cache/{z}", exist_ok=True)
                shutil.copyfile(f"renderer_sparse/cache/{z}/{x}_{y}.png", f"renderer/cache/{z}/{x}_{y}.png")
            return

        # Render the tile using the appropriate renderer executable asynchronously
        def render_tile(z, x, y):
            process = subprocess.Popen(
                [renderer_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            input_data = f"{z} {x} {y}\n".encode()
            stdout, stderr = process.communicate(input=input_data)
            process.wait()
            if process.returncode != 0:
                print(f"Error rendering tile {z}/{x}/{y}: {stderr.decode()}")
            else:
                if self.currentLayerType == "custom" and z <= 13:
                    print(f"Successfully rendered tile {z}/{x}/{y}")
                else:
                    print(f"Successfully rendered sparse tile {z}/{x}/{y}")
                if self.currentLayerType == "custom" and z > 13:
                    # copy the tile to the custom layer
                    os.makedirs(f"renderer/cache/{z}", exist_ok=True)
                    shutil.copyfile(f"renderer_sparse/cache/{z}/{x}_{y}.png", f"renderer/cache/{z}/{x}_{y}.png")
            signalBus.finishRenderingTile.emit(z, x, y)

        # Submit the rendering task to the executor
        self.executor.submit(render_tile, z, x, y)

    def on_tiles_fetched(self, z, x, y):
        pass
        # # Refresh the custom tile layer
        # self.browser.page().runJavaScript(f"""
        #     var layerId = 'tile_{z}_{x}_{y}';
        #     window.customLayerGroup.eachLayer(function (layer) {{
        #         if (layer.options.id === layerId) {{
        #             var url = layer.getTileUrl({{x: {x}, y: {y}, z: {z}}});
        #             layer.redraw();
        #             console.log("Custom tile layer refreshed with ID:", layerId);
        #         }}
        #     }});
        # """)

    @pyqtSlot(str)
    def addCustomTileLayerId(self, layer_id):
        self.custom_tile_layer_ids.append(layer_id)

    def closeEvent(self, a0):
        # Shutdown the executor
        self.executor.shutdown(wait=False)
        self.browser.page().runJavaScript("""
            window.customLayerGroup.clearLayers();
            window.baseLayerGroup.clearLayers();
        """)
        super().closeEvent(a0)

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
                if nearest_point in self.middlePoints:
                    self.middlePoints.remove(nearest_point)
                    print(f"Removed middle point at: {nearest_point}")

    # @pyqtSlot(int)
    # def toggleLayerVisibility(self, state):
    #     if state != Qt.Checked:
    #         self.browser.page().runJavaScript("""
    #             window.baseLayerGroup.eachLayer(function (layer) {
    #                 layer.setOpacity(1);
    #             });
    #             window.customLayerGroup.eachLayer(function (layer) {
    #                 layer.setOpacity(0);
    #             });
    #         """)
    #     else:
    #         self.browser.page().runJavaScript("""
    #             window.baseLayerGroup.eachLayer(function (layer) {
    #                 layer.setOpacity(0);
    #             });
    #             window.customLayerGroup.eachLayer(function (layer) {
    #                 layer.setOpacity(1);
    #             });
    #         """)

    def addBaseLayerControlButton(self):
        pass
    #     self.browser.page().runJavaScript("""
    #         if (typeof L.easyButton === 'undefined') {
    #             console.error('L.easyButton is not defined');
    #             return;
    #         }

    #         var toggleBaseLayerButton = L.easyButton({
    #             states: [{
    #                 stateName: 'show-base',
    #                 icon: '<span style="font-size: 16px;">🗺️</span>',
    #                 title: 'Hide base layer',
    #                 onClick: function(btn, map) {
    #                     btn.state('hide-base');
    #                     window.baseLayerGroup.hideLayer();
    #                 }
    #             }, {
    #                 stateName: 'hide-base',
    #                 icon: '<span style="font-size: 16px;">📍</span>',
    #                 title: 'Show base layer',
    #                 onClick: function(btn, map) {
    #                     btn.state('show-base');
    #                     window.baseLayerGroup.showLayer();
    #                 }
    #             }]
    #         });
    #         toggleBaseLayerButton.addTo(map);
    #     """)
    
    # # ...existing code...

