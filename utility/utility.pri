RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

RESOURCES += $$RELPWD/utility.qrc

HEADERS += \
  chunkreader.h \
  chunkwriter.h \
  currency.h \
  disableupdates.h \
  filter.h \
  filteredit.h \
  headerview.h \
  messagebox.h \
  multiprogressbar.h \
  progressdialog.h \
  qparallelsort.h \
  qtemporaryresource.h \
  qtemporaryresource_p.h \
  spinner.h \
  stopwatch.h \
  staticpointermodel.h \
  taskpanemanager.h \
  threadpool.h \
  transfer.h \
  undo.h \
  utility.h \
  workspace.h \


SOURCES += \
  chunkreader.cpp \
  currency.cpp \
  filter.cpp \
  filteredit.cpp \
  headerview.cpp \
  messagebox.cpp \
  multiprogressbar.cpp \
  progressdialog.cpp \
  qtemporaryresource.cpp \
  spinner.cpp \
  staticpointermodel.cpp \
  taskpanemanager.cpp \
  threadpool.cpp \
  transfer.cpp \
  undo.cpp \
  utility.cpp \
  workspace.cpp \


macx {
  HEADERS += macx.h
  OBJECTIVE_SOURCES += macx.mm
}
