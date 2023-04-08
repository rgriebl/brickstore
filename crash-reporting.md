---
layout: page
title: Crash Reporting
---
# Crash Reporting

In an effort to make BrickStore more stable, basic crash reporting support via [Sentry](https://sentry.io/) has been added. This feature is disabled by default, but since the provided information is incredibly useful in determining both the stability of the software as well as for fixing any stability issues, BrickStore builds with this feature will explicitly request it to be enabled.

Please consider enabling this feature when available. If you have any doubts about what information is being submitted and how it is used, please read the details below. Crash reporting can be enabled or disabled at any time in the Settings dialog.

## What is Collected?

First of all, no personal data, such as names, IP addresses, MAC addresses, or project and path names are collected.

When nothing goes wrong, the only thing that’s being collected is an entirely anonymous session count. The purpose of this is mainly to determine the stability of a release: the percentage of crash-free sessions.

In case of a crash, the following information is sent as part of the crash report:
 * The name and version of the operating system.
 * The version and build settings of BrickStore.
 * The make and model of your CPU and GPU. 
 * A `minidump` of the memory recently used by BrickStore. This information is used to compute a so called stack trace: it shows the list of function calls that lead up to the crash and is often the most useful bit of information when trying to fix an issue.

## How is the Information Used?

The collected information is being stored by [Sentry](https://sentry.io/) and is only accessible by BrickStore’s maintainer. It is used for the sole purpose of making BrickStore more stable.

## How Else Can I Help?

When a crash happened, please additionally check if there is already a issue reported about it on [GitHub](https://github.com/rgriebl/brickstore/issues?q=is%3Aopen+is%3Aissue). Please open a new issue when you can’t find an existing report covering your problem and describe what you did to trigger the crash.
