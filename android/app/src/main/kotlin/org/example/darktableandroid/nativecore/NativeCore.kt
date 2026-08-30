/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.nativecore

import android.content.Context
import java.io.File

object NativeCore {
    init { System.loadLibrary("dt_mobile") }
    external fun initialize(datadir: String, moduledir: String)
    fun initialize(context: Context) {
        val data = File(context.noBackupFilesDir, "darktable-runtime-v1")
        if(!data.isDirectory) {
            val staging = File(context.noBackupFilesDir, "darktable-runtime-v1.tmp")
            staging.deleteRecursively(); staging.mkdirs()
            copyAssets(context, "darktable", staging)
            check(staging.renameTo(data)) { "Unable to install darktable runtime data" }
        }
        initialize(data.absolutePath, context.applicationInfo.nativeLibraryDir)
    }

    private fun copyAssets(context: Context, path: String, destination: File) {
        val children = context.assets.list(path) ?: error("Missing runtime asset: $path")
        if(children.isEmpty()) {
            destination.parentFile?.mkdirs()
            context.assets.open(path).use { input -> destination.outputStream().use(input::copyTo) }
        } else {
            destination.mkdirs()
            children.forEach { copyAssets(context, "$path/$it", File(destination, it)) }
        }
    }
    external fun open(path: String): Long
    external fun lastError(handle: Long): String
    external fun renderPreview(handle: Long, maxWidth: Int, maxHeight: Int, dimensions: IntArray): ByteArray
    external fun cancel(handle: Long)
    external fun close(handle: Long)
}
