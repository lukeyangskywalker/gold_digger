package com.spartan.game;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Draws the Passport's 240x320 screen, scaled with nearest-neighbour filtering,
 * and owns the game thread. The native runtime is single-threaded, so input
 * from the UI thread is queued and applied on the game thread.
 */
public final class GameView extends SurfaceView implements SurfaceHolder.Callback {
    private static final int INPUT_KEY = 0;
    private static final int INPUT_KEY_CANCEL = 1;
    private static final int INPUT_POINTER = 2;
    private static final int INPUT_BACK = 3;

    private final ConcurrentLinkedQueue<int[]> inputs = new ConcurrentLinkedQueue<>();
    private final int[] pixels = new int[NativeBridge.SCREEN_W * NativeBridge.SCREEN_H];
    private final Bitmap frame =
            Bitmap.createBitmap(NativeBridge.SCREEN_W, NativeBridge.SCREEN_H, Bitmap.Config.ARGB_8888);
    private final Paint paint = new Paint();
    private final Rect source = new Rect(0, 0, NativeBridge.SCREEN_W, NativeBridge.SCREEN_H);
    private final Rect target = new Rect();
    /* Where the screen was last drawn, for mapping touches (read on the UI thread). */
    private volatile int screenLeft, screenTop, screenWidth = 1, screenHeight = 1;
    private volatile Runnable backUnhandled;
    private volatile boolean running;
    private volatile boolean surfaceReady;
    private volatile boolean saveRequested;
    private volatile boolean redrawRequested = true;
    private Thread thread;

    public GameView(Context context, AttributeSet attrs) {
        super(context, attrs);
        paint.setFilterBitmap(false);
        paint.setAntiAlias(false);
        getHolder().addCallback(this);
        setFocusable(true);
    }

    /** Queue a key edge from a keyboard or D-pad (UI thread). */
    void key(int key, boolean down) {
        inputs.add(new int[] {INPUT_KEY, key, down ? 1 : 0});
    }

    void cancel(int key) {
        inputs.add(new int[] {INPUT_KEY_CANCEL, key, 0});
    }

    /** Queue the game's Back; {@code unhandled} runs on the UI thread when the camp has nothing to return to. */
    void back(Runnable unhandled) {
        backUnhandled = unhandled;
        inputs.add(new int[] {INPUT_BACK, 0, 0});
    }

    void requestSave() {
        saveRequested = true;
    }

    void start() {
        if (running) return;
        running = true;
        thread = new Thread(this::loop, "spartan-game");
        thread.start();
    }

    /** Stops the loop after saving; blocks briefly so the save completes. */
    void stop() {
        if (!running) return;
        saveRequested = true;
        running = false;
        try {
            thread.join(2000);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private void loop() {
        boolean dirty = true;
        int lastLight = -1;
        while (running) {
            long started = System.nanoTime();
            int[] in;
            while ((in = inputs.poll()) != null) {
                switch (in[0]) {
                    case INPUT_KEY: NativeBridge.key(in[1], in[2] == 1); break;
                    case INPUT_KEY_CANCEL: NativeBridge.keyCancel(in[1]); break;
                    case INPUT_POINTER: NativeBridge.pointer(in[1], in[2], in[3]); break;
                    default:
                        Runnable unhandled = backUnhandled;
                        if (!NativeBridge.back() && unhandled != null) post(unhandled);
                        break;
                }
            }
            if (NativeBridge.tick()) dirty = true;
            if (redrawRequested) {
                redrawRequested = false;
                dirty = true;
            }
            int light = NativeBridge.backlight();
            if (light != lastLight) {
                lastLight = light;
                // The device dims its backlight; here the frame is darkened instead.
                paint.setAlpha(light >= 100 ? 255 : Math.max(40, light * 255 / 100));
                dirty = true;
            }
            if (saveRequested) {
                saveRequested = false;
                NativeBridge.save();
            }
            if (dirty && surfaceReady) {
                NativeBridge.copyFrame(pixels);
                frame.setPixels(pixels, 0, NativeBridge.SCREEN_W, 0, 0, NativeBridge.SCREEN_W,
                        NativeBridge.SCREEN_H);
                draw();
                dirty = false;
            }
            long sleepMs = 16 - (System.nanoTime() - started) / 1_000_000;
            if (sleepMs > 0) {
                try {
                    Thread.sleep(sleepMs);
                } catch (InterruptedException e) {
                    return;
                }
            }
        }
        if (saveRequested) NativeBridge.save();
    }

    private void draw() {
        SurfaceHolder holder = getHolder();
        Canvas canvas = holder.lockCanvas();
        if (canvas == null) return;
        try {
            canvas.drawColor(Color.BLACK);
            int w = canvas.getWidth();
            int h = canvas.getHeight();
            // Largest 3:4 rectangle that fits, centred.
            int scaledW = Math.min(w, h * 3 / 4);
            int scaledH = scaledW * 4 / 3;
            int left = (w - scaledW) / 2;
            int top = (h - scaledH) / 2;
            target.set(left, top, left + scaledW, top + scaledH);
            screenLeft = left;
            screenTop = top;
            screenWidth = Math.max(1, scaledW);
            screenHeight = Math.max(1, scaledH);
            canvas.drawBitmap(frame, source, target, paint);
        } finally {
            holder.unlockCanvasAndPost(canvas);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // The game draws its own controls: a tap chooses what is drawn under the
        // finger and a drag scrolls. Only the first finger is followed.
        int phase;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: phase = NativeBridge.POINTER_DOWN; break;
            case MotionEvent.ACTION_MOVE: phase = NativeBridge.POINTER_MOVE; break;
            case MotionEvent.ACTION_UP:
                phase = NativeBridge.POINTER_UP;
                performClick();
                break;
            case MotionEvent.ACTION_CANCEL: phase = NativeBridge.POINTER_CANCEL; break;
            default: return true;
        }
        int x = (int) ((event.getX() - screenLeft) * NativeBridge.SCREEN_W / screenWidth);
        int y = (int) ((event.getY() - screenTop) * NativeBridge.SCREEN_H / screenHeight);
        inputs.add(new int[] {INPUT_POINTER, phase, x, y});
        return true;
    }

    @Override
    public boolean performClick() {
        return super.performClick();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        surfaceReady = true;
        redrawRequested = true;
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        surfaceReady = true;
        redrawRequested = true;
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surfaceReady = false;
    }
}
