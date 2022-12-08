package de.brickforge.brickstore;

import org.qtproject.qt.android.bindings.QtActivity;
import android.content.res.Configuration;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

public class ExtendedQtActivity extends QtActivity
{
    public static native void changeUiTheme(boolean isDark);

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        boolean isDark = ((this.getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES);
        changeUiTheme(isDark);
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
}
