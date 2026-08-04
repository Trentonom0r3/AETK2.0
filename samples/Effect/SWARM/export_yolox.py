import sys
import os

def export_yolox():
    try:
        import torch
    except ImportError:
        print("PyTorch is required to export YOLOX models. Run: pip install torch torchvision ONNX")
        sys.exit(1)

    print("Loading YOLOX-L model from Megvii torch hub...")
    model = torch.hub.load("Megvii-BaseDetection/YOLOX", "yolox_l", pretrained=True)
    model.eval()

    dummy_input = torch.randn(1, 3, 640, 640)
    output_path = os.path.join(os.path.dirname(__file__), "yolox_l.onnx")

    print(f"Exporting YOLOX-L ONNX model to: {output_path}")
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        opset_version=16,
        input_names=["images"],
        output_names=["output"],
        dynamic_axes={"images": {0: "batch"}, "output": {0: "batch"}}
    )
    print(f"Successfully exported {output_path}")

if __name__ == "__main__":
    export_yolox()
