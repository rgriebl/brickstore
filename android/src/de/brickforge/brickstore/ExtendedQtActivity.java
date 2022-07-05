package de.brickforge.brickstore;

import org.qtproject.qt.android.bindings.QtActivity;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;


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
}
