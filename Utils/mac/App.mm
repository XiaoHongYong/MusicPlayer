#import <Foundation/Foundation.h>
#import "../App.h"
#include "../Utils.h"


const string &_getAppResourceDir() {
#ifdef _IPHONE
    NSArray *paths = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory,
        NSUserDomainMask,
        YES);
    NSString *documentsDir = [paths objectAtIndex:0];
    return(const char *)[documentsDir UTF8String];
#else
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    return [resourcePath UTF8String];
#endif
}

bool initBaseFramework(int argc, const char *argv[], cstr_t logFile, cstr_t profileName, cstr_t defAppName) {
    setAppResourceDir(_getAppResourceDir().c_str());

    string dataDir = getAppResourceDir();

    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSArray *urls = [fileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
    if ([urls count] > 0) {
        string componentName("com.crintsoft.");
        if (!isEmptyString(defAppName)) {
            componentName += defAppName;
        }
        NSURL *appSupportDir = [[urls objectAtIndex:0] URLByAppendingPathComponent:[NSString stringWithUTF8String:componentName.c_str()]];
        dataDir.assign([[appSupportDir path] UTF8String]);
        if ([fileManager fileExistsAtPath:[appSupportDir path]] != YES) {
            [fileManager createDirectoryAtURL:appSupportDir
                withIntermediateDirectories:YES
                attributes:nil
                error:nil];
        }
    }

    setAppDataDir(dataDir.c_str());

    g_profile.init(getAppDataFile(profileName).c_str(), defAppName);
    g_log.init(getAppDataFile(logFile).c_str());

    return true;
}
