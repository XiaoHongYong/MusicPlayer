#import <Foundation/Foundation.h>
#include "../HttpsGet.h"
#include "../Utils.h"


int httpGetUrl(cstr_t url, cstr_t userAgent, int &httpCode, string &body, cstr_t referer) {
    httpCode = 0;
    body.clear();
    if (isEmptyString(url)) {
        return ERR_HTTP_BAD_URL;
    }

    @autoreleasepool {
        NSString *urlStr = [NSString stringWithUTF8String:url];
        NSURL *nsUrl = [NSURL URLWithString:urlStr];
        if (nsUrl == nil) {
            return ERR_HTTP_BAD_URL;
        }

        NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:nsUrl];
        [req setHTTPMethod:@"GET"];
        [req setTimeoutInterval:30];
        [req setValue:@"*/*" forHTTPHeaderField:@"Accept"];
        if (!isEmptyString(userAgent)) {
            [req setValue:[NSString stringWithUTF8String:userAgent] forHTTPHeaderField:@"User-Agent"];
        }
        if (!isEmptyString(referer)) {
            [req setValue:[NSString stringWithUTF8String:referer] forHTTPHeaderField:@"Referer"];
        }

        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        __block int nRet = ERR_FAILED;
        __block int code = 0;
        __block string resultBody;

        // 工程为 MRC：不可把 NSData/NSURLResponse 拖出 completion，回调返回后会被释放。
        NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        NSURLSession *session = [NSURLSession sessionWithConfiguration:cfg];
        NSURLSessionDataTask *task = [session dataTaskWithRequest:req
            completionHandler:^(NSData *d, NSURLResponse *r, NSError *e) {
                if (e == nil && [r isKindOfClass:[NSHTTPURLResponse class]]) {
                    code = (int)[(NSHTTPURLResponse *)r statusCode];
                    NSUInteger len = d ? [d length] : 0;
                    if (len > 0) {
                        resultBody.assign((const char *)[d bytes], (size_t)len);
                    }
                    nRet = ERR_OK;
                }
                dispatch_semaphore_signal(sem);
            }];
        [task resume];

        long wait = dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 45ll * NSEC_PER_SEC));
        if (wait != 0) {
            [task cancel];
            [session invalidateAndCancel];
            return ERR_FAILED;
        }
        [session finishTasksAndInvalidate];

        httpCode = code;
        body.swap(resultBody);
        return nRet;
    }
}
