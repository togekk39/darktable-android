/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import android.app.Application
import android.graphics.Bitmap
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import java.nio.ByteBuffer
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.example.darktableandroid.nativecore.NativeCore
import org.example.darktableandroid.storage.UriCache

sealed interface EditorState {
    data object Empty : EditorState
    data class Working(val stage: Stage) : EditorState
    data class Ready(val source: Uri, val preview: Bitmap) : EditorState
    data class Failed(val source: Uri?, val message: String) : EditorState
    enum class Stage { COPYING, DECODING, RENDERING, CANCELLING }
}

class EditorViewModel(application: Application) : AndroidViewModel(application) {
    private val mutableState = MutableStateFlow<EditorState>(EditorState.Empty)
    val state: StateFlow<EditorState> = mutableState.asStateFlow()
    private var work: Job? = null
    private var handle = 0L
    private var generation = 0L

    fun open(uri: Uri) {
        cancel(false)
        val request = ++generation
        work = viewModelScope.launch {
            var opened = 0L
            try {
                mutableState.value = EditorState.Working(EditorState.Stage.COPYING)
                val cached = withContext(Dispatchers.IO) {
                    UriCache(getApplication()).copyForNative(uri) { request != generation }
                }
                if(request != generation) return@launch
                mutableState.value = EditorState.Working(EditorState.Stage.DECODING)
                // Keep the hand-off to this coroutine non-cancellable. Otherwise withContext can
                // discard a successfully opened handle while returning from the IO dispatcher.
                opened = withContext(NonCancellable) {
                    withContext(Dispatchers.IO) { NativeCore.open(cached.file.absolutePath) }
                }
                if(request != generation) { NativeCore.close(opened); return@launch }
                handle = opened
                mutableState.value = EditorState.Working(EditorState.Stage.RENDERING)
                val dimensions = IntArray(2)
                val rgba = withContext(Dispatchers.Default) { NativeCore.renderPreview(opened, 2048, 2048, dimensions) }
                val bitmap = Bitmap.createBitmap(dimensions[0], dimensions[1], Bitmap.Config.ARGB_8888)
                bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(rgba))
                if(request == generation) mutableState.value = EditorState.Ready(uri, bitmap) else bitmap.recycle()
            } catch(_: CancellationException) {
                // A replacement request owns the visible state.
            } catch(failure: Throwable) {
                if(request == generation) {
                    if(opened != 0L) {
                        NativeCore.close(opened)
                        if(handle == opened) handle = 0L
                        opened = 0L
                    }
                    mutableState.value = EditorState.Failed(uri, failure.message ?: "Unable to open image")
                }
            } finally {
                if(opened != 0L && request != generation) {
                    NativeCore.close(opened)
                    if(handle == opened) handle = 0L
                }
            }
        }
    }

    fun cancel(showState: Boolean = true) {
        generation++
        val cancellation = generation
        val cancelledWork = work
        if(showState) mutableState.value = EditorState.Working(EditorState.Stage.CANCELLING)
        if(handle != 0L) NativeCore.cancel(handle)
        cancelledWork?.cancel()
        if(cancelledWork?.isCompleted != false) closeSession()
        if(showState) viewModelScope.launch {
            cancelledWork?.join()
            if(generation == cancellation) {
                closeSession()
                mutableState.value = EditorState.Empty
            }
        }
    }

    private fun closeSession() {
        if(handle != 0L) { NativeCore.close(handle); handle = 0L }
    }

    override fun onCleared() { cancel(false); super.onCleared() }
}
