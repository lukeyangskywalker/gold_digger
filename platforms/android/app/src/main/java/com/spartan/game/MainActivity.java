package com.spartan.game;

import android.app.Activity;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.widget.Toast;

import java.security.SecureRandom;

/**
 * Hosts the Passport UI. Touch the screen to choose what is drawn there and
 * swipe to browse; the system Back goes back in the game (on the camp it leaves
 * the app). D-pad / keyboard keys still act as the device's Up, OK and Down
 * buttons. The game pauses with the activity and saves on every pause.
 */
public final class MainActivity extends Activity {
    private GameView game;
    private AudioTrack track;
    private Thread audioThread;
    private volatile boolean audioRunning;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (!NativeBridge.init(getFilesDir().getAbsolutePath(), new SecureRandom().nextInt(), getAssets())) {
            Toast.makeText(this, R.string.start_failed, Toast.LENGTH_LONG).show();
        }
        game = findViewById(R.id.game);
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        game.back(() -> moveTaskToBack(true));
    }

    private static int keyFor(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_W:
                return NativeBridge.KEY_UP;
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_S:
                return NativeBridge.KEY_DOWN;
            case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_SPACE:
            case KeyEvent.KEYCODE_BUTTON_A:
                return NativeBridge.KEY_OK;
            default:
                return -1;
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        int key = keyFor(keyCode);
        if (key < 0) return super.onKeyDown(keyCode, event);
        if (event.getRepeatCount() == 0) game.key(key, true);
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        int key = keyFor(keyCode);
        if (key < 0) return super.onKeyUp(keyCode, event);
        game.key(key, false);
        return true;
    }

    @Override
    protected void onResume() {
        super.onResume();
        game.start();
        startAudio();
    }

    @Override
    protected void onPause() {
        stopAudio();
        game.stop(); // saves before returning
        super.onPause();
    }

    private void startAudio() {
        int rate = NativeBridge.AUDIO_RATE_HZ;
        int min = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);
        int bytes = Math.max(min, rate / 10 * 2); // at least 100 ms
        track = new AudioTrack.Builder()
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build())
                .setAudioFormat(new AudioFormat.Builder()
                        .setSampleRate(rate)
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                        .build())
                .setBufferSizeInBytes(bytes)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .build();
        if (track.getState() != AudioTrack.STATE_INITIALIZED) {
            track.release();
            track = null;
            return;
        }
        track.play();
        audioRunning = true;
        final AudioTrack output = track;
        audioThread = new Thread(() -> {
            short[] pcm = new short[640]; // 40 ms
            while (audioRunning) {
                NativeBridge.audio(pcm, pcm.length);
                if (output.write(pcm, 0, pcm.length) < 0) break; // blocking write paces the thread
            }
        }, "spartan-audio");
        audioThread.start();
    }

    private void stopAudio() {
        audioRunning = false;
        if (track == null) return;
        track.pause();
        track.flush();
        try {
            audioThread.join(500);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        track.release();
        track = null;
    }
}
