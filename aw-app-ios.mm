
void NSApplicationMain(int argc, char *argv[]) {
	[NSApplication sharedApplication];
	[NSBundle loadNibNamed: @"myMain" owner:NSApp];
	[NSApp run];
}

