#import "AppDelegate.h"
#import "ViewControllerCommon.h"
#import "ViewController.h"
#import "ViewControllerMetal.h"
#import "iOSCoreAudio.h"
#import "Common/System/System.h"
#import "Common/System/NativeApp.h"
#import "Core/System.h"
#import "Core/Config.h"
#import "Common/Log.h"

#import <AVFoundation/AVFoundation.h>

@implementation AppDelegate

// This will be called when the user receives and dismisses a phone call
// or other interruption to the audio session
// Registered in application:didFinishLaunchingWithOptions:
// for AVAudioSessionInterruptionNotification
-(void) handleAudioSessionInterruption:(NSNotification *)notification {
	NSNumber *interruptionType = notification.userInfo[AVAudioSessionInterruptionTypeKey];

	// Sanity check in case it's somehow not an NSNumber
	if (![interruptionType respondsToSelector:@selector(unsignedIntegerValue)]) {
		return;  // Lets not crash
	}

	switch ([interruptionType unsignedIntegerValue]) {
		case AVAudioSessionInterruptionTypeBegan:
			INFO_LOG(SYSTEM, "ios audio session interruption beginning");
			if (g_Config.bEnableSound) {
				iOSCoreAudioShutdown();
			}
			break;

		case AVAudioSessionInterruptionTypeEnded:
			INFO_LOG(SYSTEM, "ios audio session interruption ending");
			if (g_Config.bEnableSound) {
				/*
				 * Only try to reinit audio if in the foreground, otherwise
				 * it may fail. Instead, trust that applicationDidBecomeActive
				 * will do it later.
				 */
				if ([UIApplication sharedApplication].applicationState == UIApplicationStateActive) {
					iOSCoreAudioInit();
				}
			}
			break;

		default:
			break;
	};
}

// This will be called when the iOS's shared media process was reset
// Registered in application:didFinishLaunchingWithOptions:
// for AVAudioSessionMediaServicesWereResetNotification
-(void) handleMediaServicesWereReset:(NSNotification *)notification {
	INFO_LOG(SYSTEM, "ios media services were reset - reinitializing audio");

	/*
	 When media services were reset, Apple recommends:
	 1) Dispose of orphaned audio objects (such as players, recorders,
	    converters, or audio queues) and create new ones
	 2) Reset any internal audio states being tracked, including all
	    properties of AVAudioSession
	 3) When appropriate, reactivate the AVAudioSession instance using the
	    setActive:error: method
	 We accomplish this by shutting down and reinitializing audio
	 */

	if (g_Config.bEnableSound) {
		iOSCoreAudioShutdown();
		iOSCoreAudioInit();
	}
}

-(BOOL) application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

	int argc = 1;
	char *argv[3] = { 0 };
	NSURL* nsUrl = [launchOptions objectForKey:UIApplicationLaunchOptionsURLKey];

	if (nsUrl != nullptr && nsUrl.isFileURL) {
		NSString *nsString = nsUrl.path;
		const char *string = nsString.UTF8String;
		argv[argc++] = (char*)string;
	}

	NSString *documentsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
	NSString *bundlePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingString:@"/assets/"];
	NativeInit(argc, (const char**)argv, documentsPath.UTF8String, bundlePath.UTF8String, NULL);

	self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

	// Choose viewcontroller depending on backend.
	if (g_Config.iGPUBackend == (int)GPUBackend::VULKAN) {
		PPSSPPViewControllerMetal *vc = [[PPSSPPViewControllerMetal alloc] init];

		self.viewController = vc;
		self.window.rootViewController = vc;

	} else {
		PPSSPPViewControllerGL *vc = [[PPSSPPViewControllerGL alloc] init];
		// Here we can switch viewcontroller depending on backend.
		self.viewController = vc;
		self.window.rootViewController = vc;
	}

	[self.window makeKeyAndVisible];

	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleAudioSessionInterruption:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleMediaServicesWereReset:) name:AVAudioSessionMediaServicesWereResetNotification object:nil];

	return YES;
}

-(void) dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self];
}

-(void) applicationWillResignActive:(UIApplication *)application {
	INFO_LOG(G3D, "willResignActive");

	[self.viewController willResignActive];
	if (g_Config.bEnableSound) {
		iOSCoreAudioShutdown();
	}

	System_PostUIMessage(UIMessage::LOST_FOCUS);
}

-(void) applicationDidBecomeActive:(UIApplication *)application {
	INFO_LOG(G3D, "didBecomeActive");
	if (g_Config.bEnableSound) {
		iOSCoreAudioInit();
	}

	System_PostUIMessage(UIMessage::GOT_FOCUS);
	[self.viewController didBecomeActive];
}

- (void)applicationWillTerminate:(UIApplication *)application {
	exit(0);
}

@end
