/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.example.darktableandroid.storage.UriCache

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val initialUri = intent?.takeIf { it.action == Intent.ACTION_VIEW || it.action == Intent.ACTION_EDIT }?.data
        setContent { MobileRawApp(initialUri, intent.flags) }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MobileRawApp(initialUri: Uri?, permissionFlags: Int) {
    var source by remember { mutableStateOf(initialUri) }
    var about by remember { mutableStateOf(false) }
    val context = androidx.compose.ui.platform.LocalContext.current
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if(uri != null) { UriCache(context).retainPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION); source = uri }
    }
    MaterialTheme {
        Scaffold(topBar = { TopAppBar(title = { Text(stringResource(R.string.app_name)) }, actions = {
            TextButton(onClick = { about = true }) { Text(stringResource(R.string.about)) }
        }) }) { padding ->
            Column(Modifier.padding(padding).padding(24.dp).fillMaxSize(), verticalArrangement = Arrangement.spacedBy(16.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                if(source == null) Text(stringResource(R.string.empty_state)) else Text("Selected source: ${source?.lastPathSegment ?: "document"}")
                Button(onClick = { picker.launch(arrayOf("image/x-adobe-dng", "image/x-canon-cr2", "image/x-canon-cr3", "image/x-nikon-nef", "image/x-sony-arw", "application/octet-stream")) }) { Text(stringResource(R.string.open_photo)) }
                Button(onClick = {}, enabled = false) { Text(stringResource(R.string.export)) }
            }
        }
        if(about) AlertDialog(onDismissRequest = { about = false }, confirmButton = { TextButton(onClick = { about = false }) { Text("OK") } }, title = { Text(stringResource(R.string.about)) }, text = {
            Text(stringResource(R.string.unofficial_notice) + "\n\nGPL-3.0-or-later\nhttps://github.com/darktable-org/darktable\nUpstream commit: 249f20eda79fdbc650e89778694c4829dba64d4b")
        })
    }
    LaunchedEffect(initialUri) { if(initialUri != null) UriCache(context).retainPermission(initialUri, permissionFlags) }
}
