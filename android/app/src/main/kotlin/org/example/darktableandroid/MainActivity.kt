/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.Image
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import org.example.darktableandroid.storage.UriCache

class MainActivity : ComponentActivity() {
    private var sourceIntent by mutableStateOf<SourceIntent?>(null)
    private var sourceIntentId = 0L

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        restoreSource(savedInstanceState)
        if(sourceIntent == null) acceptSourceIntent(intent)
        setContent { MobileRawApp(sourceIntent, ::acceptSource) }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        sourceIntent?.let { source ->
            outState.putString(SOURCE_URI, source.uri.toString())
            outState.putInt(SOURCE_FLAGS, source.permissionFlags)
            outState.putLong(SOURCE_ID, source.id)
        }
        super.onSaveInstanceState(outState)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        acceptSourceIntent(intent)
    }

    private fun acceptSourceIntent(intent: Intent) {
        if(intent.action != Intent.ACTION_VIEW && intent.action != Intent.ACTION_EDIT) return
        val uri = intent.data ?: return
        acceptSource(uri, intent.flags)
    }

    private fun acceptSource(uri: Uri, permissionFlags: Int) {
        sourceIntent = SourceIntent(uri, permissionFlags, ++sourceIntentId, shouldOpen = true)
    }

    private fun restoreSource(savedInstanceState: Bundle?) {
        val uri = savedInstanceState?.getString(SOURCE_URI)?.let(Uri::parse) ?: return
        val id = savedInstanceState.getLong(SOURCE_ID)
        sourceIntentId = id
        sourceIntent = SourceIntent(uri, savedInstanceState.getInt(SOURCE_FLAGS), id, shouldOpen = false)
    }

    private companion object {
        const val SOURCE_URI = "source_uri"
        const val SOURCE_FLAGS = "source_flags"
        const val SOURCE_ID = "source_id"
    }
}

private data class SourceIntent(
    val uri: Uri,
    val permissionFlags: Int,
    val id: Long,
    val shouldOpen: Boolean,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MobileRawApp(sourceIntent: SourceIntent?, onSourceSelected: (Uri, Int) -> Unit) {
    var about by remember { mutableStateOf(false) }
    val context = androidx.compose.ui.platform.LocalContext.current
    val editor: EditorViewModel = viewModel()
    val state by editor.state.collectAsStateWithLifecycle()
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if(uri != null) {
            UriCache(context).retainPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            onSourceSelected(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
    }
    MaterialTheme {
        Scaffold(topBar = { TopAppBar(title = { Text(stringResource(R.string.app_name)) }, actions = {
            TextButton(onClick = { about = true }) { Text(stringResource(R.string.about)) }
        }) }) { padding ->
            Column(Modifier.padding(padding).padding(24.dp).fillMaxSize(), verticalArrangement = Arrangement.spacedBy(16.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                when(val current = state) {
                    EditorState.Empty -> Text(stringResource(R.string.empty_state))
                    is EditorState.Working -> {
                        CircularProgressIndicator()
                        Text(current.stage.name.lowercase().replaceFirstChar(Char::uppercase))
                        TextButton(onClick = editor::cancel) { Text("Cancel") }
                    }
                    is EditorState.Ready -> Image(current.preview.asImageBitmap(), "Processed RAW preview", Modifier.fillMaxWidth().weight(1f), contentScale = ContentScale.Fit)
                    is EditorState.Failed -> {
                        Text(current.message, color = MaterialTheme.colorScheme.error)
                        current.source?.let { failed -> TextButton(onClick = { editor.open(failed) }) { Text("Retry") } }
                    }
                }
                Button(onClick = { picker.launch(RAW_MIME_TYPES) }) { Text(stringResource(R.string.open_photo)) }
            }
        }
        if(about) AlertDialog(onDismissRequest = { about = false }, confirmButton = { TextButton(onClick = { about = false }) { Text("OK") } }, title = { Text(stringResource(R.string.about)) }, text = {
            Text(stringResource(R.string.unofficial_notice) + "\n\nGPL-3.0-or-later\nhttps://github.com/darktable-org/darktable\nUpstream commit: 249f20eda79fdbc650e89778694c4829dba64d4b")
        })
    }
    LaunchedEffect(sourceIntent?.id) {
        sourceIntent?.takeIf { it.shouldOpen || state == EditorState.Empty }?.let {
            UriCache(context).retainPermission(it.uri, it.permissionFlags)
            editor.open(it.uri)
        }
    }
}

private val RAW_MIME_TYPES = arrayOf(
    "image/x-adobe-dng", "image/x-canon-cr2", "image/x-canon-cr3", "image/x-nikon-nef",
    "image/x-sony-arw", "image/x-fuji-raf", "image/x-olympus-orf", "image/x-panasonic-rw2",
    "application/octet-stream"
)
