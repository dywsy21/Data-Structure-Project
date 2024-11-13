# coding: utf-8
from PyQt5.QtCore import QUrl, QSize, Qt, QProcess  # Ensure Qt is imported
from PyQt5.QtGui import QIcon, QColor
from PyQt5.QtWidgets import QApplication

from qfluentwidgets import *
from qfluentwidgets import FluentIcon as FIF
from qfluentwidgets import InfoBar, InfoBarPosition  # Ensure InfoBar imports are present

from .setting_interface import SettingInterface
from .map_interface import MapInterface, QPixmap
from ..common.config import cfg
from ..common.icon import Icon
from ..common.signal_bus import signalBus
from ..common import resource


class MainWindow(FluentWindow):

    def __init__(self):
        super().__init__()
        self.initWindow()

        # 创建子界面
        self.mapInterface = MapInterface(self)
        self.settingInterface = SettingInterface(self)

        self.initBackendProcess()  # Add this line

        self.connectSignalToSlot()

        # 添加导航项
        self.initNavigation()

    def initBackendProcess(self):
        """Initialize the backend process."""
        self.backend_process = QProcess(self)
        backend_executable = "path/to/backend/executable"  # Replace with actual path

        self.backend_process.setProgram(backend_executable)
        self.backend_process.setArguments([])  # Add necessary arguments if any

        # Connect signals
        self.backend_process.readyReadStandardOutput.connect(self.handle_backend_output)
        self.backend_process.readyReadStandardError.connect(self.handle_backend_error_output)
        self.backend_process.started.connect(self.on_backend_started)
        self.backend_process.finished.connect(self.on_backend_finished)

        # Start the backend process
        self.backend_process.start()

    def handle_backend_output(self):
        """Handle output from the backend."""
        output = bytes(self.backend_process.readAllStandardOutput()).decode('utf-8')
        signalBus.backendOutputReceived.emit(output)

    def handle_backend_error_output(self):
        """Handle error output from the backend."""
        error = bytes(self.backend_process.readAllStandardError()).decode('utf-8')
        signalBus.backendErrorReceived.emit(error)

    def on_backend_finished(self, exitCode, exitStatus):
        """Handle backend process termination."""
        InfoBar.error(
            title="Backend Terminated",
            content=f"Backend exited with code {exitCode} and status {exitStatus}.",
            orient=Qt.Horizontal,
            isClosable=True,
            position=InfoBarPosition.BOTTOM_RIGHT,
            duration=2000,
            parent=self
        )

    def connectSignalToSlot(self):
        signalBus.micaEnableChanged.connect(self.setMicaEffectEnabled)
        signalBus.backendStarted.connect(self.on_backend_started)  # Add this line
        signalBus.graphLoaded.connect(self.on_graph_loaded)  # Add this line
        signalBus.sendWhitelistFlags.connect(self.send_whitelist_flags_to_backend)  # Add this line

    def send_whitelist_flags_to_backend(self, pedestrian, riding, driving, pubTransport):
        backend_command = f"{int(pedestrian)} {int(riding)} {int(driving)} {int(pubTransport)}\n"
        self.backend_process.write(backend_command.encode('utf-8'))
        self.backend_process.waitForBytesWritten()  # Ensure the data is written

    def on_backend_started(self):
        InfoBar.success(
            title="Backend Started",
            content="Backend is running in the background.",
            orient=Qt.Horizontal,
            isClosable=True,
            position=InfoBarPosition.BOTTOM_RIGHT,
            duration=2000,
            parent=self
        )

    def on_graph_loaded(self):
        InfoBar.success(
            title="Graph Loaded",
            content="Backend: Graph loaded successfully.",
            orient=Qt.Horizontal,
            isClosable=True,
            position=InfoBarPosition.BOTTOM_RIGHT,
            duration=2000,
            parent=self
        )

    def initNavigation(self):
        self.navigationInterface.setAcrylicEnabled(True)
        # 添加导航项
        self.addSubInterface(self.mapInterface, Icon.MAP, self.tr('Map'))  # 添加地图界面到导航栏
        self.addSubInterface(
            self.settingInterface, FIF.SETTING, self.tr('Settings'), NavigationItemPosition.BOTTOM)

        self.splashScreen.finish()

    def initWindow(self):
        self.resize(960, 780)
        self.setMinimumWidth(760)
        self.setWindowIcon(QIcon(Icon.APPICON.path()))
        self.setWindowTitle('Polaris')

        self.setCustomBackgroundColor(QColor(240, 244, 249), QColor(32, 32, 32))
        self.setMicaEffectEnabled(cfg.get(cfg.micaEnabled))

        # 创建启动画面
        self.splashScreen = SplashScreen(self.windowIcon(), self)
        self.splashScreen.setIconSize(QSize(106, 106))
        self.splashScreen.raise_()

        desktop = QApplication.primaryScreen().availableGeometry()
        w, h = desktop.width(), desktop.height()
        self.move(w//2 - self.width()//2, h//2 - self.height()//2)
        self.show()
        QApplication.processEvents()

    def resizeEvent(self, e):
        super().resizeEvent(e)
        if hasattr(self, 'splashScreen'):
            self.splashScreen.resize(self.size())

