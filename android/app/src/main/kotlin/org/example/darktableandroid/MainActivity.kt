/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.Image
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import org.example.darktableandroid.nativecore.ExportFormat
import org.example.darktableandroid.nativecore.ExportSettings
import org.example.darktableandroid.nativecore.EditorModule
import org.example.darktableandroid.nativecore.ModuleParameter
import org.example.darktableandroid.nativecore.CurvePoint
import org.example.darktableandroid.nativecore.CropRect
import org.example.darktableandroid.nativecore.parseNumericInput
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
    var tools by remember { mutableStateOf(false) }
    var exportSettings by remember { mutableStateOf(false) }
    var formatId by rememberSaveable { mutableStateOf("") }
    var chosenSettings by rememberSaveable { mutableStateOf(ExportSettings().toNativeArray()) }
    var pendingSessionId by rememberSaveable { mutableLongStateOf(-1L) }
    var pendingSourceUri by rememberSaveable { mutableStateOf("") }
    var pendingFormatId by rememberSaveable { mutableStateOf("") }
    var pendingFormatName by rememberSaveable { mutableStateOf("") }
    var pendingExtension by rememberSaveable { mutableStateOf("") }
    var pendingMimeType by rememberSaveable { mutableStateOf("application/octet-stream") }
    var pendingSettings by rememberSaveable { mutableStateOf(ExportSettings().toNativeArray()) }
    var pendingExtended by rememberSaveable { mutableStateOf(false) }
    val context = LocalContext.current
    val editor: EditorViewModel = viewModel()
    val state by editor.state.collectAsStateWithLifecycle()
    val runtime by editor.runtime.collectAsStateWithLifecycle()
    val ready = when(val current = state) {
        is EditorState.Ready -> current
        is EditorState.Working -> current.previous
        else -> null
    }
    val busy = state is EditorState.Working
    val selectedFormat = ready?.exportFormats?.firstOrNull { it.id == formatId } ?: ready?.exportFormats?.firstOrNull()
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if(uri != null) {
            UriCache(context).retainPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            onSourceSelected(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
    }
    val exportPicker = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if(result.resultCode == Activity.RESULT_OK && pendingSessionId >= 0L) {
            result.data?.data?.let { target ->
                val format = ExportFormat(pendingFormatId, pendingFormatName, pendingExtension, pendingMimeType, null)
                if(pendingExtended) editor.export(target, format, pendingSettings,
                    pendingSessionId, Uri.parse(pendingSourceUri))
                else editor.export(target, format, ExportSettings.fromNativeArray(pendingSettings)?.quality ?: 95,
                    pendingSessionId, Uri.parse(pendingSourceUri))
            }
        }
        pendingSessionId = -1L
    }
    MaterialTheme {
        Scaffold(topBar = { TopAppBar(title = { Text(stringResource(R.string.app_name)) }, actions = {
            TextButton(onClick = { about = true }) { Text(stringResource(R.string.about)) }
        }) }) { padding ->
            Column(Modifier.padding(padding).padding(24.dp).fillMaxSize(), verticalArrangement = Arrangement.spacedBy(16.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                when {
                    runtime.checking -> CircularProgressIndicator()
                    !runtime.available -> Text(runtime.message.orEmpty(), color = MaterialTheme.colorScheme.error)
                    state == EditorState.Empty -> Text(stringResource(R.string.empty_state))
                }
                ready?.let { current ->
                    Image(current.preview.asImageBitmap(), stringResource(R.string.preview_description),
                        Modifier.fillMaxWidth().weight(1f), contentScale = ContentScale.Fit)
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        TextButton(onClick = editor::undo, enabled = !busy && current.history?.canUndo == true) {
                            Text(stringResource(R.string.undo))
                        }
                        TextButton(onClick = editor::redo, enabled = !busy && current.history?.canRedo == true) {
                            Text(stringResource(R.string.redo))
                        }
                    }
                    current.notice?.let { Text(it) }
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        Button(onClick = { tools = true }, enabled = !busy) { Text(stringResource(R.string.edit_tools)) }
                        Button(onClick = { exportSettings = true }, enabled = !busy && current.exportFormats.isNotEmpty()) {
                            Text(stringResource(R.string.export))
                        }
                    }
                }
                when(val current = state) {
                    is EditorState.Working -> {
                        LinearProgressIndicator(Modifier.fillMaxWidth())
                        Text(stringResource(current.stage.labelResource()))
                        TextButton(onClick = { editor.cancel() }, enabled = current.stage != EditorState.Stage.CANCELLING) {
                            Text(stringResource(R.string.cancel))
                        }
                    }
                    is EditorState.Failed -> {
                        Text(current.message, color = MaterialTheme.colorScheme.error)
                        current.source?.let { failed -> TextButton(onClick = { editor.open(failed) }) { Text(stringResource(R.string.retry)) } }
                    }
                    else -> Unit
                }
                Button(onClick = { picker.launch(arrayOf("*/*")) }, enabled = runtime.available && !busy) {
                    Text(stringResource(R.string.open_photo))
                }
            }
        }
        if(tools && ready != null) ModalBottomSheet(onDismissRequest = { tools = false }) {
            Column(Modifier.fillMaxHeight(0.8f).padding(horizontal = 24.dp)) {
                Text(stringResource(R.string.edit_tools), style = MaterialTheme.typography.titleLarge)
                Text(stringResource(R.string.tool_limitations), style = MaterialTheme.typography.bodySmall)
                ready.notice?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                LazyColumn(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(ready.modules, key = { "${ready.sessionId}:${it.operation}" }) { module ->
                        ModuleControls(module, !busy, editor::setModuleEnabled, editor::setParameter,
                            editor::setCurve, editor::setCrop, editor::setRgbCurveOptions,
                            editor::setWhiteBalanceGains, editor::setWhiteBalanceSetting)
                    }
                }
            }
        }
        if(exportSettings && ready != null && selectedFormat != null) ExportDialog(
            formats = ready.exportFormats,
            initialFormatId = selectedFormat.id,
            initialSettings = ExportSettings.fromNativeArray(chosenSettings) ?: ExportSettings(),
            enabled = !busy,
            onDismiss = { exportSettings = false },
            onChoose = { format, settings ->
                exportSettings = false
                formatId = format.id
                chosenSettings = settings.toNativeArray()
                // Capture all preferences and the source identity before leaving this activity.
                pendingSessionId = ready.sessionId
                pendingSourceUri = ready.source.toString()
                pendingFormatId = format.id
                pendingFormatName = format.name
                pendingExtension = format.extension
                pendingMimeType = format.mimeType
                pendingSettings = settings.toNativeArray()
                pendingExtended = format.options != null
                exportPicker.launch(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                    addCategory(Intent.CATEGORY_OPENABLE)
                    type = format.mimeType
                    putExtra(Intent.EXTRA_TITLE, "edited.${format.extension}")
                })
            },
        )
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

@Composable
private fun ModuleControls(
    module: EditorModule,
    editable: Boolean,
    onEnabled: (String, Boolean) -> Unit,
    onParameter: (String, String, Double) -> Unit,
    onCurve: (String, Int, Int, List<CurvePoint>) -> Unit,
    onCrop: (String, CropRect) -> Unit,
    onRgbCurveOptions: (String, Int, Int) -> Unit,
    onWhiteBalanceGains: (String, FloatArray) -> Unit,
    onWhiteBalanceSetting: (String, Int) -> Unit,
) {
    var expanded by rememberSaveable(module.operation) { mutableStateOf(false) }
    OutlinedCard(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = { expanded = !expanded }, modifier = Modifier.weight(1f)) { Text(module.name) }
                Switch(checked = module.enabled, onCheckedChange = { onEnabled(module.operation, it) }, enabled = editable && module.canDisable)
            }
            if(expanded) {
                if(module.parameters.isEmpty() && module.curves.isEmpty() && module.crop == null && module.rgbCurveOptions == null && module.whiteBalance == null)
                    Text(stringResource(R.string.no_numeric_parameters))
                module.crop?.let { crop ->
                    CropEditor(crop, editable) { bounds -> onCrop(module.operation, bounds) }
                }
                module.rgbCurveOptions?.let { options ->
                    RgbCurveOptionsEditor(options, editable) { mode, preserve ->
                        onRgbCurveOptions(module.operation, mode, preserve)
                    }
                }
                module.whiteBalance?.let { balance ->
                    WhiteBalanceEditor(balance, editable,
                        { gains -> onWhiteBalanceGains(module.operation, gains) },
                        { setting -> onWhiteBalanceSetting(module.operation, setting) })
                }
                if(module.curves.isNotEmpty()) ModuleCurves(module.curves, editable) { channel, interpolation, nodes ->
                    onCurve(module.operation, channel, interpolation, nodes)
                }
                module.parameters.forEach { parameter ->
                    key(parameter.name) {
                        ParameterControl(parameter, editable) { onParameter(module.operation, parameter.name, it) }
                    }
                }
            }
        }
    }
}

@Composable
private fun ParameterControl(parameter: ModuleParameter, editable: Boolean, onValue: (Double) -> Unit) {
    when {
        parameter.type == "bool" || parameter.type == "boolean" -> Row(verticalAlignment = Alignment.CenterVertically) {
            Text(parameter.label, Modifier.weight(1f))
            Switch(checked = parameter.value != 0.0, onCheckedChange = { onValue(if(it) 1.0 else 0.0) }, enabled = editable)
        }
        parameter.choices.isNotEmpty() -> {
            var expanded by remember { mutableStateOf(false) }
            Text(parameter.label, style = MaterialTheme.typography.labelLarge)
            Box {
                OutlinedButton(onClick = { expanded = true }, enabled = editable) {
                    Text(parameter.choices.firstOrNull { it.value == parameter.value }?.label ?: parameter.value.toString())
                }
                DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                    parameter.choices.forEach { choice ->
                        DropdownMenuItem(text = { Text(choice.label) }, enabled = editable,
                            onClick = { expanded = false; onValue(choice.value) })
                    }
                }
            }
        }
        else -> {
            var draft by remember(parameter.name, parameter.value) { mutableStateOf(parameter.value.toString()) }
            val parsed = parameter.parseNumericInput(draft)
            val valid = parsed != null
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(value = draft, onValueChange = { draft = it }, label = { Text(parameter.label) },
                    modifier = Modifier.weight(1f), enabled = editable, singleLine = true, isError = !valid,
                    // Android decimal keyboards can omit the minus sign. Use
                    // ASCII for signed ranges so exposure and offsets remain editable.
                    keyboardOptions = KeyboardOptions(keyboardType =
                        if(parameter.minimum == null || parameter.minimum < 0.0) KeyboardType.Ascii else KeyboardType.Decimal),
                    supportingText = {
                        if(parameter.minimum != null || parameter.maximum != null) {
                            Text("${parameter.minimum ?: "−∞"} … ${parameter.maximum ?: "∞"}")
                        }
                    })
                TextButton(onClick = { parsed?.let(onValue) }, enabled = editable && valid && parsed != parameter.value) {
                    Text(stringResource(R.string.apply))
                }
            }
        }
    }
}

private fun EditorState.Stage.labelResource(): Int = when(this) {
    EditorState.Stage.COPYING -> R.string.stage_copying
    EditorState.Stage.DECODING -> R.string.stage_decoding
    EditorState.Stage.RENDERING -> R.string.stage_rendering
    EditorState.Stage.EDITING -> R.string.stage_editing
    EditorState.Stage.EXPORTING -> R.string.stage_exporting
    EditorState.Stage.CANCELLING -> R.string.stage_cancelling
}
