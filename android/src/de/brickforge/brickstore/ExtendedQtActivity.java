// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

package de.brickforge.brickstore;

import org.qtproject.qt.android.bindings.QtActivity;
import android.content.res.Configuration;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import io.sentry.SentryLevel;
import io.sentry.android.core.SentryAndroid;

public class ExtendedQtActivity extends QtActivity
{
    public static native void changeUiTheme(boolean isDark);

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        boolean isDark = ((this.getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES);
        changeUiTheme(isDark);

        Intent intent = getIntent();
        if ((intent != null) && (intent.getAction() == Intent.ACTION_VIEW))
            openUrl(intent.getData().toString());
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        super.onConfigurationChanged(newConfig);

        boolean isDark = ((newConfig.uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES);
        changeUiTheme(isDark);
    }

    public static native void openUrl(String url);

    @Override
    public void onNewIntent(Intent intent)
    {
        if (intent.getAction() == Intent.ACTION_VIEW) {
            openUrl(intent.getData().toString());
        } else {
            super.onNewIntent(intent);
        }
    }

    public String getFileName(String uriStr)
    {
        String fileName = null;
        Uri uri = Uri.parse(uriStr);
        if (uri.getScheme().equals("content")) {
            Cursor cursor = getContentResolver().query(uri, null, null, null, null);
            try {
                if (cursor != null && cursor.moveToFirst())
                    fileName = cursor.getString(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
            } finally {
                cursor.close();
            }
        }
        if (fileName == null) {
            fileName = uri.getPath();
            int idx = fileName.lastIndexOf('/');
            if (idx != -1)
                fileName = fileName.substring(idx + 1);
        }
        return fileName;
    }

    public boolean setupSentry(String dsn, String release, boolean debug, boolean userConsentRequired)
    {
        //TODO: userConsentRequired is not supported by sentry-android yet

        SentryAndroid.init(this, options -> {
            options.setDsn(dsn);
            options.setRelease(release);
            options.setDebug(debug);
        });
        return true;
    }
}
