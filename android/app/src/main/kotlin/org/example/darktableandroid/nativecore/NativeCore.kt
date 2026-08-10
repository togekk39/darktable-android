/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.nativecore

object NativeCore {
    init { System.loadLibrary("dt_mobile") }
    external fun open(path: String): Long
    external fun lastError(handle: Long): String
    external fun renderPreview(handle: Long, maxWidth: Int, maxHeight: Int, dimensions: IntArray): ByteArray
    external fun cancel(handle: Long)
    external fun close(handle: Long)
}
