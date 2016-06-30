import sys
import Gears as gears
import importlib.machinery
import os
import atexit
import traceback
from PyQt5.QtCore import (Qt, QCoreApplication, QUrl)
from PyQt5.QtGui import (QSurfaceFormat)
from PyQt5.QtWidgets import (QApplication, QMessageBox, QSpacerItem, QSizePolicy)
from PyQt5.QtMultimediaWidgets import QVideoWidget
from PyQt5.QtMultimedia import (QMediaPlayer, QMediaPlaylist)
from PyQt5.QtNetwork import QAuthenticator 

from StimulusWindow import StimulusWindow
#from MediaWindow import MediaWindow
from LauncherWindow import LauncherWindow
from BrowserWindow import BrowserWindow

try:
  from OpenGL.GL import *
  from OpenGL.GLU import *
except:
  print ('ERROR: PyOpenGL not installed properly.')


try:
    Qt.AA_ShareOpenGLContexts = True
    app = QApplication(sys.argv)
    app.setKeyboardInputInterval(0)

    #glFormat = QSurfaceFormat()
    #glFormat.setMajorVersion(4)
    #glFormat.setMinorVersion(4)
    #QSurfaceFormat.setDefaultFormat( glFormat )

#    videoWidget = None
#    mediaPlayer= QMediaPlayer(None, QMediaPlayer.VideoSurface)
    
#    stimulusWindow = StimulusWindow()
#    mediaWindow = MediaWindow(stimulusWindow)
    gears.createStimulusWindow()
    launcherWindow = LauncherWindow()
    browserWindow = BrowserWindow(launcherWindow, app)

    atexit.register(gears.cleanup)
    sys.exit(app.exec_())
except SystemExit:
    pass
except:
    exc_type, exc_value, exc_traceback = sys.exc_info()
    formatted_lines = traceback.format_exc().splitlines()
    box = QMessageBox(None)
    box.setStyleSheet("""
            QWidget{
                border-style: solid;
                border-width: 1px;
                border-radius: 0px;
                border-color: red;
                background-color: black;
                font: bold 14px;
                color: red;
                font-family: "Candara";
                font-size: 14px;
            }
            """)
    horizontalSpacer = QSpacerItem(1000, 0, QSizePolicy.Minimum, QSizePolicy.Expanding)
    message = ''
    for l in formatted_lines :
        message += l + '\n'
    box.setText( message )
    box.setWindowTitle('Python error!')
    box.setWindowFlags(Qt.Dialog);
    box.setStandardButtons(QMessageBox.Abort)
    box.setDefaultButton(QMessageBox.Abort)
    layout = box.layout()
    layout.addItem(horizontalSpacer, layout.rowCount(), 0, 1, layout.columnCount())
    box.exec()

