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
    sendBackendRequest = pyqtSignal(str)  # Already added
    graphLoaded = pyqtSignal()  # Add this line
    updateLocationSuggestions = pyqtSignal(list)

signalBus = SignalBus()