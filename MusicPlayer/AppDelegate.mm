//
//  AppDelegate.m
//  MusicPlayer
//
//  Created by HongyongXiao on 2021/11/11.
//

#import "AppDelegate.h"

void CallMenuCommand(int cmd);

// This is used to handle the main menu of MusicPlayer
@interface MainMenuHandler : NSObject {
}

- (void) onCommand:(NSMenuItem *)item;

@end

@implementation MainMenuHandler

- (void) onCommand:(NSMenuItem *)item
{
    CallMenuCommand((int)[item tag]);
}

-(BOOL) validateMenuItem:(NSMenuItem *)item
{
    return YES;
}

@end

#import "../MPlayerUI/MPlayerApp.h"
#import "../MPlayerUI/MPSkinMenu.h"
#import "../Utils/LocalizeTool.h"
// #import "../Window/WindowLib.h"


void CallMenuCommand(int cmd) {
    MPlayerApp::getInstance()->getMainWnd()->onCommand(cmd);
}

@implementation AppDelegate

@synthesize window;

SkinMenuPtr _menu;
MainMenuHandler *_mainMenuHandler = [MainMenuHandler alloc];

NSMenuItem *duplicateMenuItem(NSMenuItem *org) {
    if ([org isSeparatorItem])
        return [NSMenuItem separatorItem];

    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[org title]
                                                  action:[org action]
                                           keyEquivalent:[org keyEquivalent]];

    [item setTarget:_mainMenuHandler];
    [item setTag:[org tag]];

    if ([org hasSubmenu] != YES)
        return item;

    NSMenu *subMenu = [org submenu];
    NSMenu *newSubMenu = [[NSMenu alloc] initWithTitle:[org title]];
    [newSubMenu setAutoenablesItems:NO];
    [item setSubmenu:newSubMenu];

    int count = (int)[subMenu numberOfItems];
    for (int i = 0; i < count; i++) {
        NSMenuItem *newSubItem = duplicateMenuItem([subMenu itemAtIndex:i]);
        [newSubMenu addItem:newSubItem];
    }

    [newSubMenu release];

    return item;
}

static const NSInteger kSkinMenuItemTag = 0x4D504D4E;

static NSString *nsUtf8(cstr_t text) {
    if (!text) {
        return @"";
    }
    return [NSString stringWithUTF8String:text];
}

static void localizeAppleMenu(NSMenu *mainMenu) {
    if ([mainMenu numberOfItems] < 1) {
        return;
    }

    NSMenu *appMenu = [[mainMenu itemAtIndex:0] submenu];
    for (NSMenuItem *item in [appMenu itemArray]) {
        if ([item isSeparatorItem]) {
            continue;
        }
        SEL act = [item action];
        if (act == @selector(aboutWindow:) || act == @selector(orderFrontStandardAboutPanel:)) {
            [item setTitle:nsUtf8(_TL("about $Product$"))];
        } else if (act == @selector(preferencesWindow:)) {
            [item setTitle:nsUtf8(_TL("Preferences"))];
        } else if (act == @selector(hide:)) {
            [item setTitle:nsUtf8(_TL("Hide $Product$"))];
        } else if (act == @selector(hideOtherApplications:)) {
            [item setTitle:nsUtf8(_TL("Hide Others"))];
        } else if (act == @selector(unhideAllApplications:)) {
            [item setTitle:nsUtf8(_TL("Show All"))];
        } else if (act == @selector(terminate:)) {
            [item setTitle:nsUtf8(_TL("Quit $Product$"))];
        }
    }
}

static void stripXibStubMenus(NSMenu *mainMenu) {
    NSSet *stubs = [NSSet setWithObjects:
        @"File", @"文件",
        @"Format", @"格式",
        @"View", @"显示", @"视图",
        @"Help", @"帮助",
        nil];

    for (NSInteger i = [mainMenu numberOfItems] - 1; i >= 0; i--) {
        NSMenuItem *item = [mainMenu itemAtIndex:i];
        if ([item tag] == kSkinMenuItemTag || [stubs containsObject:[item title]]) {
            [mainMenu removeItemAtIndex:i];
        }
    }
}

static void installMacApplicationMenu() {
    MPlayerApp *pApp = MPlayerApp::getInstance();
    NSMenu *mainMenu = [NSApp mainMenu];
    if (!mainMenu) {
        return;
    }

    stripXibStubMenus(mainMenu);
    localizeAppleMenu(mainMenu);

    for (NSMenuItem *item in [mainMenu itemArray]) {
        NSString *title = [item title];
        if ([title isEqualToString:@"Window"] || [title isEqualToString:@"窗口"]) {
            [item setTitle:nsUtf8(_TL("Window"))];
            [[item submenu] setTitle:[item title]];
            for (NSMenuItem *sub in [[item submenu] itemArray]) {
                if ([sub isSeparatorItem]) {
                    continue;
                }
                SEL act = [sub action];
                if (act == @selector(performMiniaturize:)) {
                    [sub setTitle:nsUtf8(_TL("Minimize"))];
                } else if (act == @selector(performZoom:)) {
                    [sub setTitle:nsUtf8(_TL("Zoom"))];
                } else if (act == @selector(arrangeInFront:)) {
                    [sub setTitle:nsUtf8(_TL("Bring All to Front"))];
                }
            }
        }
    }

    const char *szMenu = "MainWndMenu";
    _menu = pApp->getSkinFactory()->loadMenu(pApp->getMainWnd(), szMenu);
    if (!_menu) {
        return;
    }

    _menu->updateMenuStatus(pApp->getMainWnd());
    NSMenu *menu = (NSMenu *)_menu->getHandle(pApp->getMainWnd());
    for (int i = 0; i < [menu numberOfItems]; i++) {
        NSMenuItem *item = duplicateMenuItem([menu itemAtIndex:i]);
        [item setTag:kSkinMenuItemTag];
        if ([item hasSubmenu]) {
            [item submenu].delegate = (id<NSMenuDelegate>)[NSApp delegate];
        }
        [mainMenu addItem:item];
        [item release];
    }

    _menu->attachHandle(mainMenu);
}

void reloadMacApplicationMenu() {
    installMacApplicationMenu();
}

- (void)menuWillOpen:(NSMenu *)menu {
    _menu->updateMenuStatus(MPlayerApp::getInstance()->getMainWnd());
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    MPlayerApp *pApp = MPlayerApp::getInstance();
    pApp->init();
    installMacApplicationMenu();
}

/**
    Returns the directory the application uses to store the Core Data store file. This code uses a directory named "MusicPlayerMac" in the user's Library directory.
 */
- (NSURL *)applicationFilesDirectory {

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *libraryURL = [[fileManager URLsForDirectory:NSLibraryDirectory inDomains:NSUserDomainMask] lastObject];
    return [libraryURL URLByAppendingPathComponent:@"MusicPlayer"];
}

/**
    Creates if necessary and returns the managed object model for the application.
 */
- (NSManagedObjectModel *)managedObjectModel {
    if (__managedObjectModel) {
        return __managedObjectModel;
    }

    NSURL *modelURL = [[NSBundle mainBundle] URLForResource:@"MusicPlayer" withExtension:@"momd"];
    __managedObjectModel = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    return __managedObjectModel;
}

/**
    Returns the persistent store coordinator for the application. This implementation creates and return a coordinator, having added the store for the application to it. (The directory for the store is created, if necessary.)
 */
- (NSPersistentStoreCoordinator *)persistentStoreCoordinator {
    if (__persistentStoreCoordinator) {
        return __persistentStoreCoordinator;
    }

    NSManagedObjectModel *mom = [self managedObjectModel];
    if (!mom) {
        NSLog(@"%@:%@ No model to generate a store from", [self class], NSStringFromSelector(_cmd));
        return nil;
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *applicationFilesDirectory = [self applicationFilesDirectory];
    NSError *error = nil;

    NSDictionary *properties = [applicationFilesDirectory resourceValuesForKeys:[NSArray arrayWithObject:NSURLIsDirectoryKey] error:&error];

    if (!properties) {
        BOOL ok = NO;
        if ([error code] == NSFileReadNoSuchFileError) {
            ok = [fileManager createDirectoryAtPath:[applicationFilesDirectory path] withIntermediateDirectories:YES attributes:nil error:&error];
        }
        if (!ok) {
            [[NSApplication sharedApplication] presentError:error];
            return nil;
        }
    }
    else {
        if ([[properties objectForKey:NSURLIsDirectoryKey] boolValue] != YES) {
            // Customize and localize this error.
            NSString *failureDescription = [NSString stringWithFormat:@"Expected a folder to store application data, found a file (%@).", [applicationFilesDirectory path]];

            NSMutableDictionary *dict = [NSMutableDictionary dictionary];
            [dict setValue:failureDescription forKey:NSLocalizedDescriptionKey];
            error = [NSError errorWithDomain:@"YOUR_ERROR_DOMAIN" code:101 userInfo:dict];

            [[NSApplication sharedApplication] presentError:error];
            return nil;
        }
    }

    NSURL *url = [applicationFilesDirectory URLByAppendingPathComponent:@"MusicPlayerMac.storedata"];
    __persistentStoreCoordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:mom];
    if (![__persistentStoreCoordinator addPersistentStoreWithType:NSXMLStoreType configuration:nil URL:url options:nil error:&error]) {
        [[NSApplication sharedApplication] presentError:error];
        [__persistentStoreCoordinator release];
        __persistentStoreCoordinator = nil;
        return nil;
    }

    return __persistentStoreCoordinator;
}

/**
    Returns the managed object context for the application (which is already
    bound to the persistent store coordinator for the application.)
 */
- (NSManagedObjectContext *)managedObjectContext {
    if (__managedObjectContext) {
        return __managedObjectContext;
    }

    NSPersistentStoreCoordinator *coordinator = [self persistentStoreCoordinator];
    if (!coordinator) {
        NSMutableDictionary *dict = [NSMutableDictionary dictionary];
        [dict setValue:@"Failed to initialize the store" forKey:NSLocalizedDescriptionKey];
        [dict setValue:@"There was an error building up the data file." forKey:NSLocalizedFailureReasonErrorKey];
        NSError *error = [NSError errorWithDomain:@"YOUR_ERROR_DOMAIN" code:9999 userInfo:dict];
        [[NSApplication sharedApplication] presentError:error];
        return nil;
    }
    __managedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
    [__managedObjectContext setPersistentStoreCoordinator:coordinator];

    return __managedObjectContext;
}

/**
    Returns the NSUndoManager for the application. In this case, the manager returned is that of the managed object context for the application.
 */
- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    return [[self managedObjectContext] undoManager];
}

/**
    Performs the save action for the application, which is to send the save: message to the application's managed object context. Any encountered errors are presented to the user.
 */
- (IBAction)saveAction:(id)sender {
    NSError *error = nil;

    if (![[self managedObjectContext] commitEditing]) {
        NSLog(@"%@:%@ unable to commit editing before saving", [self class], NSStringFromSelector(_cmd));
    }

    if (![[self managedObjectContext] save:&error]) {
        [[NSApplication sharedApplication] presentError:error];
    }
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {

    // Save changes in the application's managed object context before the application terminates.

    MPlayerApp::getInstance()->quit();

    if (!__managedObjectContext) {
        return NSTerminateNow;
    }

    if (![[self managedObjectContext] commitEditing]) {
        NSLog(@"%@:%@ unable to commit editing to terminate", [self class], NSStringFromSelector(_cmd));
        return NSTerminateCancel;
    }

    if (![[self managedObjectContext] hasChanges]) {
        return NSTerminateNow;
    }

    NSError *error = nil;
    if (![[self managedObjectContext] save:&error]) {

        // Customize this code block to include application-specific recovery steps.
        BOOL result = [sender presentError:error];
        if (result) {
            return NSTerminateCancel;
        }

        NSString *question = NSLocalizedString(@"Could not save changes while quitting. Quit anyway?", @"Quit without saves error question message");
        NSString *info = NSLocalizedString(@"Quitting now will lose any changes you have made since the last successful save", @"Quit without saves error question info");
        NSString *quitButton = NSLocalizedString(@"Quit anyway", @"Quit anyway button title");
        NSString *cancelButton = NSLocalizedString(@"Cancel", @"Cancel button title");
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:question];
        [alert setInformativeText:info];
        [alert addButtonWithTitle:quitButton];
        [alert addButtonWithTitle:cancelButton];

        NSInteger answer = [alert runModal];
        [alert release];
        alert = nil;

        if (answer == NSAlertSecondButtonReturn) {
            return NSTerminateCancel;
        }
    }

    return NSTerminateNow;
}

- (IBAction)aboutWindow:(id)sender
{
    CMPSkinMainWnd *pWnd = MPlayerApp::getInstance()->getMainWnd();
    if (pWnd != NULL && pWnd->isValid())
        pWnd->postCustomCommandMsg(ID_ABOUT);
}

- (IBAction)preferencesWindow:(id)sender
{
    CMPSkinMainWnd *pWnd = MPlayerApp::getInstance()->getMainWnd();
    if (pWnd != NULL && pWnd->isValid())
        pWnd->postCustomCommandMsg(ID_PREFERENCES);
}

- (void)dealloc
{
    [__managedObjectContext release];
    [__persistentStoreCoordinator release];
    [__managedObjectModel release];
    [super dealloc];
}


@end
