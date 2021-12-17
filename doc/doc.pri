bsver.name = VERSION
bsver.value = $$VERSION
qtdocs.name = QT_INSTALL_DOCS
qtdocs.value = $$[QT_INSTALL_DOCS]
QT_TOOL_ENV = bsver qtdocs
qtPrepareTool(QDOC, qdoc)
QT_TOOL_ENV =

extensions_doc.commands = $$QDOC "$$PWD/extensions.qdocconf" -indexdir "$$[QT_INSTALL_DOCS]"
QMAKE_EXTRA_TARGETS += extensions_doc

OTHER_FILES += \
  $$PWD/extensions.qdoc \
  $$PWD/extensions.qdocconf \
  $$PWD/extensions.css \
  $$PWD/images/list_arrow.png \
  $$PWD/images/list_expand.png \
  $$PWD/images/favicon.ico \
