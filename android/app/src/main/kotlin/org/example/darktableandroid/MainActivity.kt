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
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
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
import org.example.darktableandroid.nativecore.sliderBounds
import org.example.darktableandroid.nativecore.sliderInput
import org.example.darktableandroid.nativecore.resetValue
import org.example.darktableandroid.nativecore.hasEditableControls
import org.example.darktableandroid.nativecore.controlsFirst
import org.example.darktableandroid.nativecore.initialControlModule
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
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
                        Button(onClick = {
                            if(current.selectedModule == null) editor.selectModule(current.modules.initialControlModule())
                            tools = true
                        }, enabled = !busy) { Text(stringResource(R.string.edit_tools)) }
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
        if(tools && ready != null) ModalBottomSheet(onDismissRequest = { tools = false },
            sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)) {
            val moduleList = rememberLazyListState()
            var toolQuery by rememberSaveable(ready.sessionId) { mutableStateOf("") }
            var enabledOnly by rememberSaveable(ready.sessionId) { mutableStateOf(false) }
            val shownModules = remember(ready.modules, toolQuery, enabledOnly) {
                filterEditingTools(ready.modules, toolQuery, enabledOnly).controlsFirst()
            }
            LaunchedEffect(ready.sessionId, ready.selectedModule, shownModules) {
                val index = shownModules.indexOfFirst { it.operation == ready.selectedModule }
                if(index >= 0 && moduleList.layoutInfo.visibleItemsInfo.none { it.key == "${ready.sessionId}:${ready.selectedModule}" })
                    moduleList.animateScrollToItem(index)
            }
            Column(Modifier.fillMaxHeight(0.9f).imePadding().padding(horizontal = 24.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.edit_tools), style = MaterialTheme.typography.titleLarge, modifier = Modifier.weight(1f))
                    TextButton(onClick = { tools = false }) { Text(stringResource(R.string.done)) }
                }
                EditingToolSearch(toolQuery, { toolQuery = it }, enabledOnly, { enabledOnly = it },
                    shownModules.size, ready.modules.size)
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = editor::undo, enabled = !busy && ready.history?.canUndo == true) {
                        Text(stringResource(R.string.undo))
                    }
                    TextButton(onClick = editor::redo, enabled = !busy && ready.history?.canRedo == true) {
                        Text(stringResource(R.string.redo))
                    }
                }
                (state as? EditorState.Working)?.let { working ->
                    LinearProgressIndicator(Modifier.fillMaxWidth())
                    Text(stringResource(working.stage.labelResource()))
                    TextButton(onClick = editor::cancel, enabled = working.stage != EditorState.Stage.CANCELLING) {
                        Text(stringResource(R.string.cancel))
                    }
                }
                ready.notice?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                LazyColumn(Modifier.fillMaxWidth().weight(1f), state = moduleList, verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    if(shownModules.isEmpty()) item { Text(stringResource(R.string.tools_no_matches), Modifier.padding(vertical = 16.dp)) }
                    items(shownModules, key = { "${ready.sessionId}:${it.operation}" }) { module ->
                        ModuleControls(module, !busy, ready.selectedModule == module.operation,
                            editor::selectModule, editor::newInstance, editor::setModuleEnabled, editor::setParameter,
                            editor::setCurve, editor::setCrop, editor::setRgbCurveOptions,
                            editor::setWhiteBalanceGains, editor::setWhiteBalanceSetting, editor::setWhiteBalanceTemperature,
                            editor::setTextParameter, controlQuery = toolQuery,
                            onFileParameter = editor::importToolFile, sessionId = ready.sessionId)
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
internal fun ModuleControls(
    module: EditorModule,
    editable: Boolean,
    selected: Boolean,
    onSelected: (String?) -> Unit,
    onNewInstance: (String) -> Unit,
    onEnabled: (String, Boolean) -> Unit,
    onParameter: (String, String, Double) -> Unit,
    onCurve: (String, Int, Int, List<CurvePoint>) -> Unit,
    onCrop: (String, CropRect) -> Unit,
    onRgbCurveOptions: (String, Int, Int) -> Unit,
    onWhiteBalanceGains: (String, FloatArray) -> Unit,
    onWhiteBalanceSetting: (String, Int) -> Unit,
    onWhiteBalanceTemperature: (String, Double, Double) -> Unit,
    onTextParameter: (String, String, String) -> Unit = { _, _, _ -> },
    controlQuery: String = "",
    onFileParameter: (String, String, Uri, Long) -> Unit = { _, _, _, _ -> },
    sessionId: Long = -1L,
) {
    var expanded by rememberSaveable(module.operation) { mutableStateOf(false) }
    LaunchedEffect(selected) { if(selected) expanded = true }
    val disclosure = stringResource(if(expanded) R.string.module_collapse_controls else R.string.module_expand_controls, module.name)
    val operation = module.operation.substringBefore('@')
    OutlinedCard(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = {
                    expanded = !expanded
                    if(expanded) onSelected(module.operation) else if(selected) onSelected(null)
                },
                    enabled = editable, modifier = Modifier.weight(1f).semantics { contentDescription = disclosure }) {
                    Column(horizontalAlignment = Alignment.Start) {
                        Text(module.name)
                        Text(stringResource(if(expanded) R.string.module_hide_controls else if(module.hasEditableControls)
                            R.string.module_show_controls else R.string.module_show_details), style = MaterialTheme.typography.labelSmall)
                    }
                }
                Switch(checked = module.enabled, onCheckedChange = {
                    if(it) { expanded = true; onSelected(module.operation) }
                    onEnabled(module.operation, it)
                }, enabled = editable && module.canDisable)
            }
            if(operation == "liquify")
                Text(stringResource(if(module.parameters.any { it.name.startsWith("liquify.") })
                    R.string.liquify_point_scope else R.string.liquify_canvas_scope), style = MaterialTheme.typography.bodySmall)
            if(operation == "colortransfer")
                Text(stringResource(R.string.colortransfer_scope), style = MaterialTheme.typography.bodySmall)
            else if(!module.hasEditableControls)
                Text(stringResource(R.string.no_numeric_parameters), style = MaterialTheme.typography.bodySmall)
            if(expanded) {
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
                        { setting -> onWhiteBalanceSetting(module.operation, setting) },
                        { kelvin, tint -> onWhiteBalanceTemperature(module.operation, kelvin, tint) })
                }
                if(module.curves.isNotEmpty()) ModuleCurves(module.curves, editable) { channel, interpolation, nodes ->
                    onCurve(module.operation, channel, interpolation, nodes)
                }
                val controlWords = controlSearchWords(module, controlQuery)
                val visibleParameters = module.parameters.filter { matchesControlSearch(controlWords, it.name, it.label) }
                val adjustments = visibleParameters.filterNot { it.name.startsWith("blend.") || it.name.startsWith("mask.") }
                val blending = visibleParameters.filter { it.name.startsWith("blend.") }
                val masks = visibleParameters.filter { it.name.startsWith("mask.") }
                // Original liquify node deletion compacts indexes. Recreate its drafts
                // after native changes so an old index cannot address another warp.
                val controlIdentity = if(operation == "liquify") module.parameters else null
                if(adjustments.isNotEmpty()) {
                    Text(stringResource(R.string.module_adjustments), style = MaterialTheme.typography.titleSmall)
                    adjustments.forEach { parameter ->
                        key(parameter.name, controlIdentity) {
                            ParameterControl(parameter, editable) { onParameter(module.operation, parameter.name, it) }
                        }
                    }
                }
                module.textParameters.filter { matchesControlSearch(controlWords, it.name, it.label) }.forEach { parameter ->
                    key(parameter.name) {
                        if(parameter.isFile) FileParameterControl(parameter, editable, sessionId) { uri, expectedSessionId ->
                            onFileParameter(module.operation, parameter.name, uri, expectedSessionId)
                        } else TextParameterControl(parameter, editable) { onTextParameter(module.operation, parameter.name, it) }
                    }
                }
                if(blending.isNotEmpty()) {
                    HorizontalDivider()
                    Text(stringResource(R.string.module_blending), style = MaterialTheme.typography.titleSmall)
                    blending.forEach { parameter ->
                        key(parameter.name) {
                            ParameterControl(parameter, editable) { onParameter(module.operation, parameter.name, it) }
                        }
                    }
                }
                if(masks.isNotEmpty()) {
                    HorizontalDivider()
                    val shapeTitle = if(module.operation.substringBefore('@') in setOf("retouch", "spots"))
                        R.string.module_repair_shapes else R.string.module_drawn_masks
                    Text(stringResource(shapeTitle), style = MaterialTheme.typography.titleSmall)
                    Text(stringResource(R.string.mask_geometry_hint), style = MaterialTheme.typography.bodySmall)
                    MaskDiagram(module.parameters.filter { it.name.startsWith("mask.") })
                    masks.forEach { parameter ->
                        key(parameter.name) {
                            ParameterControl(parameter, editable) { onParameter(module.operation, parameter.name, it) }
                        }
                    }
                }
                if(module.canReset) {
                    HorizontalDivider()
                    val description = stringResource(R.string.module_reset_description, module.name)
                    OutlinedButton(onClick = { onParameter(module.operation, "module.reset", 1.0) }, enabled = editable,
                        modifier = Modifier.semantics { contentDescription = description }) {
                        Text(stringResource(R.string.module_reset))
                    }
                }
                if(module.canCreateInstance) {
                    TextButton(onClick = { onNewInstance(module.operation) }, enabled = editable) {
                        Text(stringResource(R.string.module_new_instance))
                    }
                    Text(stringResource(R.string.module_new_instance_hint), style = MaterialTheme.typography.bodySmall)
                }
            }
        }
    }
}

@Composable
internal fun ParameterControl(parameter: ModuleParameter, editable: Boolean, onValue: (Double) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        when {
            parameter.type == "action" -> Button(onClick = { onValue(1.0) }, enabled = editable) {
                Text(parameter.label)
            }
            parameter.type == "bool" || parameter.type == "boolean" -> Row(verticalAlignment = Alignment.CenterVertically) {
                Text(parameter.label, Modifier.weight(1f))
                Switch(checked = parameter.value != 0.0, onCheckedChange = { onValue(if(it) 1.0 else 0.0) },
                    enabled = editable, modifier = Modifier.semantics { contentDescription = parameter.label })
            }
            parameter.choices.isNotEmpty() -> {
                var expanded by remember { mutableStateOf(false) }
                Text(parameter.label, style = MaterialTheme.typography.labelLarge)
                Box {
                    OutlinedButton(onClick = { expanded = true }, enabled = editable,
                        modifier = Modifier.semantics { contentDescription = parameter.label }) {
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
                parameter.sliderBounds()?.let { bounds ->
                    ParameterSlider(parameter.label, parsed ?: parameter.value, bounds.start, bounds.endInclusive,
                        editable, onValue = { draft = parameter.sliderInput(it) }, onFinished = {
                            // One history/preview operation per completed drag. Reads the current
                            // state directly, including accessibility SetProgress actions.
                            parameter.parseNumericInput(draft)?.takeIf { it != parameter.value }?.let(onValue)
                        })
                }
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(value = draft, onValueChange = { draft = it }, label = { Text(parameter.label) },
                        modifier = Modifier.weight(1f), enabled = editable, singleLine = true, isError = parsed == null,
                        // Android decimal keyboards can omit minus. Keep signed input available.
                        keyboardOptions = KeyboardOptions(keyboardType =
                            if(parameter.minimum == null || parameter.minimum < 0.0) KeyboardType.Ascii else KeyboardType.Decimal),
                        supportingText = {
                            if(parameter.minimum != null || parameter.maximum != null)
                                Text("${parameter.minimum ?: "−∞"} … ${parameter.maximum ?: "∞"}")
                        })
                    TextButton(onClick = { parameter.parseNumericInput(draft)?.let(onValue) },
                        enabled = editable && parsed != null && parsed != parameter.value) {
                        Text(stringResource(R.string.apply))
                    }
                }
            }
        }
        parameter.resetValue()?.takeIf { parameter.type != "action" }?.let { default ->
            val description = stringResource(R.string.parameter_reset_description, parameter.label)
            TextButton(onClick = { onValue(default) }, enabled = editable && default != parameter.value,
                modifier = Modifier.semantics { contentDescription = description }) {
                Text(stringResource(R.string.parameter_reset))
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
