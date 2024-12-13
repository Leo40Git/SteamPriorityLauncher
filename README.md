# SteamPriorityLauncher
Launches a Steam game, then sets its priority.

## Installation
1. Download the latest EXE from [here](https://github.com/Leo40Git/SteamPriorityLauncher/releases/latest).  
**Note:** If you've already done this once, you don't need to do it again. Just create a new shortcut to the same EXE.
2. Right-click on the EXE, and then select "Create shortcut".
3. Right-click on the newly created shortcut, and then select "Properties".
4. In the "Target" textbox, add the following:  
`-gameID <Steam game ID> -gameExe <name of the game's main EXE>`  
If you don't know the name of the game's main EXE, run the game normally, open Task Manager, and look for the game in the list. Then, right-click on it and select "Properties". You should see the EXE name in the first textbox.  
Example:  
![Example Image](https://github.com/Leo40Git/SteamPriorityLauncher/raw/master/example.png)  
Contents of "Target": `SteamPriorityLauncher.exe -gameID 440 -gameExe tf.exe -priority A`  
This will launch [Team Fortress 2 (440)](https://store.steampowered.com/app/440) with priority "Above Normal".  
If you're on 64-bit Windows, use `-gameExe tf_win64.exe` instead.
5. Optionally, rename the shortcut and give it a proper icon.
6. Select "OK".
7. **You're done!**

## Options
To see the launcher's options, simply run it using your preferred command line with no options.

### `-gameID`
The Steam ID of the game you want to launch. *Required.*

### `-gameExe`
The name of the game's main EXE. *Required.*

### `-priority`
The game process' new priority.  
Valid values are:
  - L - Low/Idle*
  - B - Below Normal*
  - N - Normal
  - A - Above Normal
  - H - High*
  - R - Realtime* (requires admin privileges, may cause system instability!)

  (* - not recommended)  
If this option is not specified, the priority will be set to A (Above Normal).

### `-affinity`
What cores the game process will be allowed to run on.  
This option takes a list of cores, where 0 is core #1. Every number in the list is separated by a semicolon (`;`).  
If this option is not specified, the process will be allowed to run on all cores.  
Entries that aren't valid decimal numbers are evaluated to 0.  
Invalid entries (I.E selecting core 4 [actually 5] on a quad-core processor) are ignored.  
If the given core list evaluates to nothing, the launcher will error out.  
You won't need to set this option in most cases.  
