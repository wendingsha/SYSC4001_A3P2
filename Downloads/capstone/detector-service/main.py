from fastapi import FastAPI, UploadFile, File, HTTPException
from ultralytics import YOLO
from PIL import Image
import io
import numpy as np
import cv2
import os
from dotenv import load_dotenv

# Load environment variables from .env file
load_dotenv()

app = FastAPI(title="YOLO Object Detection Service")

# Load the model (downloads nano model by default on first run)
# In production, you would likely load a custom model from the ./models directory
MODEL_NAME = os.getenv("YOLO_MODEL", "models/best.pt")

try:
    print(f"Loading model: {MODEL_NAME}")
    if MODEL_NAME == "models/best.pt" and not os.path.exists(MODEL_NAME):
        print(f"Custom model {MODEL_NAME} not found. Falling back to yolov8n.pt")
        MODEL_NAME = "yolov8n.pt"
        
    model = YOLO(MODEL_NAME) 
except Exception as e:
    print(f"Error loading model: {e}")
    model = None

@app.get("/")
async def root():
    return {"message": "Object Detection Service is running", "model": MODEL_NAME}

@app.post("/predict")
async def predict(file: UploadFile = File(...)):
    if not model:
        raise HTTPException(status_code=500, detail="Model not initialized")

    try:
        # Read image file
        contents = await file.read()
        image = Image.open(io.BytesIO(contents))
        
        # Run inference
        results = model(image)
        
        # Process results
        detections = []
        for result in results:
            for box in result.boxes:
                detections.append({
                    "class": model.names[int(box.cls)],
                    "confidence": float(box.conf),
                    "bbox": box.xyxy.tolist()[0]  # [x1, y1, x2, y2]
                })
                
        return {
            "filename": file.filename,
            "detections": detections,
            "count": len(detections)
        }
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error processing image: {str(e)}")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)

