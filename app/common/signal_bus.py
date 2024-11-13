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
    sendBackendRequest = pyqtSignal(str, bool, bool, bool, bool)  # Update to include four flags
    graphLoaded = pyqtSignal()  # Add this line
    updateLocationSuggestions = pyqtSignal(list)
    sendWhitelistFlags = pyqtSignal(bool, bool, bool, bool)  # Already present

signalBus = SignalBus()