# Object Detection Service

This service provides a REST API for object detection using YOLOv8.

## Stack

- **Language:** Python 3.11.9
- **Framework:** FastAPI
- **ML Model:** YOLOv8 (Ultralytics)
- **Container:** Docker

## Model Configuration

The service is configured by default to look for a custom model at `models/best.pt`.

1. **Commit your model:** Place your `best.pt` in the `models/` folder and commit it to the repository.
   *(Note: Since the model is small (<100MB), we are committing it directly to Git so it gets built into the Docker image automatically.)*
2. **Run:** The service will automatically load it.

*Fallback:* If `models/best.pt` is not found, the service will automatically fall back to the standard `yolov8n.pt` (Nano) model.

## CI/CD (GitHub Actions)

This project is configured to automatically build and push the Docker image to Docker Hub when changes are pushed to the `main` branch.

### Setup Secrets
To enable the workflow, you must add the following secrets to your GitHub repository settings (`Settings` -> `Secrets and variables` -> `Actions`):

1. `DOCKER_USERNAME`: Your Docker Hub username.
2. `DOCKER_PASSWORD`: Your Docker Hub access token (preferred) or password.

## Setup

### Local Development

1. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

2. Run the server:
   ```bash
   python main.py
   ```
   The API will be available at `http://localhost:8000`.
   Interactive docs: `http://localhost:8000/docs`.

### Docker

1. Build the image:
   ```bash
   docker build -t detector-service .
   ```

2. Run (mounting the models folder):
   ```bash
   docker run -p 8000:8000 -v ${PWD}/models:/app/models detector-service
   ```
   *Note: The `-v` flag is crucial so the container can see the `best.pt` file on your host machine.*

## API Usage

### `POST /predict`

Upload an image file to detect objects.

**Example using cURL:**

```bash
curl -X POST "http://localhost:8000/predict" \
  -H "accept: application/json" \
  -H "Content-Type: multipart/form-data" \
  -F "file=@/path/to/your/image.jpg"
```
