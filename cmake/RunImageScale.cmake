# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# imagescale_run(<label> <arguments...>)
#
# Runs the imagescale tool from ${IMAGESCALE} and turns a failure into an error
# that says what actually went wrong. Included by the generator scripts, which run
# standalone under `cmake -P`, so everything here has to stay scriptable.
# BuildImageScale.cmake is the counterpart that provides ${IMAGESCALE} itself.
#
# The tool is built well away from Qt and links against it dynamically, so the
# most likely failure by far is that Qt's library directory is not in the
# loader's path. On Windows that surfaces as exit code 0xc0000135
# (STATUS_DLL_NOT_FOUND) with nothing at all on stdout or stderr, so without the
# hint below the build stops on an error that names neither Qt nor PATH.

include_guard(GLOBAL)

function(imagescale_run label)
    # A missing binary and one that cannot load its libraries both come back
    # empty-handed, but they need opposite advice, so rule this one out up front.
    if (NOT EXISTS "${IMAGESCALE}")
        message(FATAL_ERROR
            "imagescale is missing, cannot generate ${label}\n"
            "  expected at: ${IMAGESCALE}\n"
            "The generators depend on the imagescale target, so a normal build "
            "cannot get here. Seeing this means the build directory was cleaned "
            "under a running generator, or IMAGESCALE points somewhere stale."
        )
    endif()

    execute_process(
        COMMAND "${IMAGESCALE}" ${ARGN}
        RESULT_VARIABLE result
        ERROR_VARIABLE stderr
    )
    if (result EQUAL 0)
        return()
    endif()

    string(STRIP "${stderr}" stderr)
    string(REPLACE ";" " " command "${IMAGESCALE};${ARGN}")

    set(hint "")
    if (NOT stderr)
        # exited without a word: 0xc0000135 on Windows, 126/127 from a shell
        set(hint
            "\nThe tool did not produce any output, so it most likely never ran. It "
            "links against Qt dynamically - make sure Qt's library directory is in "
            "PATH (Windows), LD_LIBRARY_PATH (Linux) or DYLD_LIBRARY_PATH (macOS)."
        )
        string(JOIN "" hint ${hint})
    endif()

    message(FATAL_ERROR
        "imagescale failed for ${label} (${result})\n"
        "  ${command}\n"
        "${stderr}${hint}"
    )
endfunction()
