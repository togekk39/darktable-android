/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.storage

import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.net.Uri
import java.io.File
import java.security.MessageDigest

class UriCache(private val context: Context, private val maxBytes: Long = 512L * 1024 * 1024) {
    data class CachedSource(val file: File, val sha256: String)

    fun retainPermission(uri: Uri, flags: Int) {
        val allowed = flags and Intent.FLAG_GRANT_READ_URI_PERMISSION
        if (allowed != 0) runCatching { context.contentResolver.takePersistableUriPermission(uri, allowed) }
    }

    fun copyForNative(uri: Uri): CachedSource {
        require(uri.scheme == ContentResolver.SCHEME_CONTENT) { "Only content URIs are accepted" }
        val directory = File(context.cacheDir, "raw-sources").apply { mkdirs() }
        pruneCache(directory, maxBytes)
        val temporary = File.createTempFile("incoming-", ".raw", directory)
        val digest = MessageDigest.getInstance("SHA-256")
        try {
            context.contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "Provider returned no input stream" }
                temporary.outputStream().buffered().use { output ->
                    val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                    var total = 0L
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        total += count
                        if (total > maxBytes) error("Source exceeds the ${maxBytes / (1024 * 1024)} MiB cache limit")
                        digest.update(buffer, 0, count); output.write(buffer, 0, count)
                    }
                }
            }
            val hash = digest.digest().joinToString("") { "%02x".format(it) }
            val stable = File(directory, "$hash.raw")
            if (!stable.exists()) check(temporary.renameTo(stable)) { "Unable to finalize cached source" } else temporary.delete()
            pruneCache(directory, maxBytes, stable)
            return CachedSource(stable, hash)
        } catch (failure: Throwable) { temporary.delete(); throw failure }
    }
}

internal fun pruneCache(directory: File, maxBytes: Long, retained: File? = null) {
    val files = directory.listFiles()?.sortedBy { it.lastModified() } ?: return
    var size = files.sumOf { it.length() }
    for (file in files) {
        if (size <= maxBytes) break
        val fileSize = file.length()
        if (file != retained && file.delete()) size -= fileSize
    }
    check(size <= maxBytes) { "Unable to reduce source cache to its configured limit" }
}
