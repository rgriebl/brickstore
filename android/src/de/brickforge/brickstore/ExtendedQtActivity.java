// Copyright (C) 2004-2025 Robert Griebl
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
import androidx.core.view.WindowCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.core.graphics.Insets;
import io.sentry.SentryLevel;
import io.sentry.android.core.SentryAndroid;

public class ExtendedQtActivity extends QtActivity
{
    public static native void changeScreenMargins(int left, int top, int right, int bottom);

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        // ViewCompat.getWindowInsetsController(getWindow().getDecorView()).setAppearanceLightNavigationBars(false);
        ViewCompat.setOnApplyWindowInsetsListener(getWindow().getDecorView(), (view, windowInsets) -> {
            int insetsTypes = WindowInsetsCompat.Type.displayCutout() | WindowInsetsCompat.Type.systemBars();
            Insets insets = windowInsets.getInsets(insetsTypes);
            changeScreenMargins(insets.left, insets.top, insets.right, insets.bottom);
            return WindowInsetsCompat.CONSUMED;
        });

        Intent intent = getIntent();
        if ((intent != null) && (intent.getAction() == Intent.ACTION_VIEW))
            openUrl(intent.getData().toString());
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        super.onConfigurationChanged(newConfig);
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

    public boolean isSideLoaded()
    {
        try {
            return getPackageManager().getInstallerPackageName(getPackageName()) == null;
        } catch (Throwable e) {
            return false;
        }
    }
}
