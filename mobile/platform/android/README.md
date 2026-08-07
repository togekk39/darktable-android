<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android platform adapter

Android URI access stays in Kotlin. Seekable cache paths are passed through JNI; a future adapter may accept duplicated `ParcelFileDescriptor` file descriptors without exposing Android types in the core API.
