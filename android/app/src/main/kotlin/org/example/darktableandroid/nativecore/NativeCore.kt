/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.nativecore

import android.content.Context
import java.io.File

object NativeCore {
    private data class RuntimePaths(val datadir: String, val moduledir: String)

    private val initializationLock = Any()
    @Volatile private var initializedPaths: RuntimePaths? = null

    init { System.loadLibrary("dt_mobile") }
    external fun initialize(datadir: String, moduledir: String)
    fun initialize(context: Context) {
        val data = File(context.noBackupFilesDir, "darktable-runtime-v1")
        val paths = RuntimePaths(data.absolutePath, context.applicationInfo.nativeLibraryDir)
        if(initializedPaths == paths) return

        synchronized(initializationLock) {
            if(initializedPaths == paths) return
            check(initializedPaths == null) {
                "Native runtime is already initialized with different paths"
            }
            if(!data.isDirectory) {
                val staging = File(context.noBackupFilesDir, "darktable-runtime-v1.tmp")
                staging.deleteRecursively(); staging.mkdirs()
                copyAssets(context, "darktable", staging)
                check(staging.renameTo(data)) { "Unable to install darktable runtime data" }
            }
            initialize(paths.datadir, paths.moduledir)
            initializedPaths = paths
        }
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
