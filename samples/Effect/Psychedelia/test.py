import cv2
import numpy as np

def generate_noise_layer(h, w, scale):
    small_w = max(1, int(w / scale))
    small_h = max(1, int(h / scale))
    noise = np.random.rand(small_h, small_w).astype(np.float32)
    return cv2.resize(noise, (w, h), interpolation=cv2.INTER_CUBIC)

def hash2d(x, y):
    dot = x * 12.9898 + y * 78.233
    return (np.sin(dot) * 43758.5453) % 1.0

def get_dmt_palette(t):
    """Inigo Quilez Cosine Palette for intense, cyclical gradients."""
    t_exp = np.expand_dims(t, axis=2)
    r = 0.5 + 0.5 * np.cos(6.28318 * (1.0 * t_exp + 0.00))
    g = 0.5 + 0.5 * np.cos(6.28318 * (1.0 * t_exp + 0.33))
    b = 0.5 + 0.5 * np.cos(6.28318 * (1.0 * t_exp + 0.67))
    return np.concatenate([b, g, r], axis=2) * 255.0

def get_dominant_colors(image, k=5):
    """Extracts dominant colors from an image using K-Means clustering."""
    # Shrink heavily for speed; we only need rough color averages
    small_img = cv2.resize(image, (100, 100))
    pixels = np.float32(small_img.reshape(-1, 3))
    
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 10, 1.0)
    _, labels, centers = cv2.kmeans(pixels, k, None, criteria, 10, cv2.KMEANS_RANDOM_CENTERS)
    
    # Sort colors by perceived luminance to ensure smooth gradient flow
    luminance = np.sum(centers * np.array([0.114, 0.587, 0.299]), axis=1)
    centers = centers[np.argsort(luminance)]
    return centers

def get_custom_palette(t, colors):
    """Linearly interpolates across a discrete set of colors using a continuous phase (t)."""
    k = len(colors)
    # Slow down the color cycling slightly so the source colors are more apparent
    t_mod = np.mod(t * (k * 0.5), k) 
    
    idx1 = np.floor(t_mod).astype(int)
    idx2 = (idx1 + 1) % k
    frac = np.expand_dims(t_mod - idx1, axis=2)
    
    # NumPy advanced indexing maps the 2D grid to the 1D color array instantly
    c1 = colors[idx1]
    c2 = colors[idx2]
    
    return (c1 * (1.0 - frac)) + (c2 * frac)

def apply_mandala(wx, wy, mandala_val, cx, cy, f_a):
    if mandala_val <= 0.01:
        return wx, wy 
        
    segments = np.floor(1.0 + (mandala_val * 11.0)) * 2.0 
    dx, dy = wx - cx, wy - cy
    
    r = np.sqrt(dx**2 + dy**2)
    theta = np.arctan2(dy, dx)
    
    angle_step = (2.0 * np.pi) / segments
    theta = np.mod(theta, angle_step)
    theta = np.abs(theta - (angle_step / 2.0))
    theta += (f_a * 0.5)
    
    return cx + r * np.cos(theta), cy + r * np.sin(theta)

def main(img):
    if img is None:
        print("Error: No image provided.")
        return
        
    img = cv2.resize(img, (800, 800))
    h, w = img.shape[:2]
    
    print("Extracting Source Palette...")
    source_colors = get_dominant_colors(img, k=5)
    
    print("Generating Noise Fields...")
    n1 = generate_noise_layer(h, w, 50.0)
    n2 = generate_noise_layer(h, w, 25.0)
    f_a = (n1 + n2 * 0.5) / 1.5

    xx, yy = np.meshgrid(np.arange(w, dtype=np.float32), np.arange(h, dtype=np.float32))
    
    # --- EXTRACT STRUCTURAL DATA ---
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
    
    smooth_source = cv2.GaussianBlur(gray, (21, 21), 0)
    grad_x = cv2.Sobel(smooth_source, cv2.CV_32F, 1, 0, ksize=5)
    grad_y = cv2.Sobel(smooth_source, cv2.CV_32F, 0, 1, ksize=5)
    
    sharp_x = cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3)
    sharp_y = cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3)
    edges = np.sqrt(sharp_x**2 + sharp_y**2)
    edges = edges / (np.max(edges) + 1e-5) 

    def update(val):
        grid_scale_val = cv2.getTrackbarPos("1. Base Grid Scale", "Psychedelia") / 100.0
        mandala_val = cv2.getTrackbarPos("2. Mandala Folds", "Psychedelia") / 100.0
        source_influence = cv2.getTrackbarPos("3. Source Influence", "Psychedelia") / 100.0
        mix_level = cv2.getTrackbarPos("4. Real/Math Mix", "Psychedelia") / 100.0
        use_source_colors = cv2.getTrackbarPos("5. Use Source Colors (0/1)", "Psychedelia")
        
        # DISPLACEMENT WARP
        warp_power = source_influence * 150.0
        wx = xx + (grad_x * warp_power) + ((f_a - 0.5) * 20.0)
        wy = yy + (grad_y * warp_power) + ((f_a - 0.5) * 20.0)
        
        # MANDALA FOLD
        cx, cy = w / 2.0, h / 2.0
        wx, wy = apply_mandala(wx, wy, mandala_val, cx, cy, f_a)
        
        # ISOMETRIC SKEW
        wx_skew = wx + (wy * 0.5)
        wy_skew = wy
        
        # ADAPTIVE GRID RESOLUTION
        base_grid = 5.0 + (1.0 - grid_scale_val) * 60.0
        shrink_factor = 1.0 - (edges * source_influence * 0.8)
        local_grid = np.maximum(base_grid * shrink_factor, 3.0)
        
        cell_x = np.floor(wx_skew / local_grid)
        cell_y = np.floor(wy_skew / local_grid)
        cell_hash = hash2d(cell_x, cell_y)
        
        # CONTOUR PHASE MAPPING
        contour_strength = source_influence * 3.0
        color_phase = cell_hash + (gray * contour_strength)
        
        # --- NEW: DYNAMIC PALETTE SELECTION ---
        if use_source_colors > 0:
            psychedelic_colors = get_custom_palette(color_phase, source_colors)
        else:
            psychedelic_colors = get_dmt_palette(color_phase)

        # STRUCTURAL EDGE BURN
        burn = 1.0 - (edges * source_influence * 0.7) 
        burn_map = np.expand_dims(burn, axis=2)
        
        psychedelic_frame = np.clip(psychedelic_colors * burn_map, 0, 255).astype(np.uint8)

        # LERP MIX
        out_img = cv2.addWeighted(psychedelic_frame, mix_level, img, 1.0 - mix_level, 0)

        cv2.imshow("Psychedelia", out_img)

    cv2.namedWindow("Psychedelia", cv2.WINDOW_AUTOSIZE)
    cv2.createTrackbar("1. Base Grid Scale", "Psychedelia", 50, 100, update)
    cv2.createTrackbar("2. Mandala Folds", "Psychedelia", 0, 100, update)
    cv2.createTrackbar("3. Source Influence", "Psychedelia", 60, 100, update) 
    cv2.createTrackbar("4. Real/Math Mix", "Psychedelia", 100, 100, update) 
    cv2.createTrackbar("5. Use Source Colors (0/1)", "Psychedelia", 1, 1, update) # Checkbox toggle
    
    update(0)
    print("Window is open. Drag the sliders. Press any key in the window to exit.")
    cv2.waitKey(0)
    cv2.destroyAllWindows()

if __name__ == "__main__":
    videofile = "C:\\Users\\tjerf\\Downloads\\8513823-uhd_3840_2160_25fps.mp4" 
    cap = cv2.VideoCapture(videofile)
    if cap.isOpened():
        cap.set(cv2.CAP_PROP_POS_FRAMES, 50)
        ret, frame = cap.read()
        cap.release() 
        if ret:
            main(frame)