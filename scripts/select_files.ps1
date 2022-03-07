
# Powershell script to launch GUI file picker on Windows.
# Writes file selection to a file which can be read separately.
#
# Run like:
#   powershell -ExecutionPolicy ByPass -NoLogo -File .\select.ps1


# Interactively select files using the native Windows open file dialog.
Function SelectFiles($initialDirectory) {
    [System.Reflection.Assembly]::LoadWithPartialName("System.windows.forms") | Out-Null
    
    $OpenFileDialog = New-Object System.Windows.Forms.OpenFileDialog
    $OpenFileDialog.initialDirectory = $initialDirectory
    $OpenFileDialog.filter = "All files (*.*)| *.*"

    # Bug workaround where the dialog can hang when the prompt is created.
    # Note that some of these properties may not be applicable for the file dialog.
    $OpenFileDialog.ShowHelp = $True
    $OpenFileDialog.MultiSelect = $True
    #$openFileDialog.OverwritePrompt = $True
    #$openFileDialog.CreatePrompt = $True

    $OpenFileDialog.ShowDialog() | Out-Null
    return $OpenFileDialog.FileNames
}

$selectedFiles = SelectFiles "C:\" | Out-String


# Write the list of files to a file.
#Write-Host "$selectedFiles"
Out-File -InputObject $selectedFiles -Encoding utf8 -FilePath .\files.txt
