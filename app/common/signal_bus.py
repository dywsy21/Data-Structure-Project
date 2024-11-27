# coding: utf-8
from PyQt5.QtCore import QObject, pyqtSignal


class SignalBus(QObject):
    """ Signal bus """

    checkUpdateSig = pyqtSignal()
    micaEnableChanged = pyqtSignal(bool)
    visitSourceCodeSig = pyqtSignal()
    backendOutputReceived = pyqtSignal(str)
    backendErrorReceived = pyqtSignal(str)
    backendStarted = pyqtSignal()
    sendBackendRequest = pyqtSignal(str)  # Update to include four flags
    graphLoaded = pyqtSignal()  # Add this line
    updateLocationSuggestions = pyqtSignal(list)
    sendWhitelistFlags = pyqtSignal(bool, bool, bool, bool)  # Already present
    renderTile = pyqtSignal(int, int, int) # z, x, y
    finishRenderingTile = pyqtSignal(int, int, int)  # z, x, y

signalBus = SignalBus()