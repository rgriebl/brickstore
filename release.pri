unix:RELEASE = $$system(cat RELEASE)
win32:RELEASE = $$system(type RELEASE)

eval(RELEASE_SPLIT=$$replace(RELEASE, "\\.", " "))

RELEASE_MAJOR = $$member(RELEASE_SPLIT, 0)
RELEASE_MINOR = $$member(RELEASE_SPLIT, 1)
RELEASE_PATCH = $$member(RELEASE_SPLIT, 2)

win32:RELEASE_USER = $$(USERNAME)
unix:RELEASE_USER = $$(USER)
win32:RELEASE_HOST = $$(COMPUTERNAME)
unix:RELEASE_HOST = $$system(hostname)

