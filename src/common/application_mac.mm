#include <QString>
#include <QByteArray>
#include <QDebug>
#include <exception>
#import <Foundation/NSException.h>
#import <Foundation/NSJSONSerialization.h>

bool macDecodeNSException()
{
    @try {
        std::rethrow_exception(std::current_exception());
    } @catch (NSException *nse) {
        QByteArray userInfo;
        if (nse.userInfo) {
            const auto ui = static_cast<NSDictionary * _Nonnull>(nse.userInfo);
            if (NSData *nsd = [NSJSONSerialization dataWithJSONObject:ui options:kNilOptions error:nil])
                userInfo = QByteArray::fromNSData(nsd);
        }
        qWarning().noquote() << "uncaught NSException, name:" << QString::fromNSString(nse.name)
                             << "\n  reason:" << QString::fromNSString(nse.reason)
                             << "\n  userInfo:" << userInfo;
        return true;
    } @catch (...) {
        return false;
    }
}
