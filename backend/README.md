# Backend (Flask)

1. In backend folder, create your local env file:
   - `cp .env.example .env`
   - edit `.env` with MongoDB URI (find on MongoDB atlas)
2. Activate virtual environment
   - `source .venv/bin/activate`
3. Install packages:
   - `pip install flask flask-cors pymongo python-dotenv requests`
4. Start the backend API:
   - `python server.py`
5. In a second terminal, start mock sensor data:
   - `python mock_generator.py`

API runs on `http://localhost:5001`
