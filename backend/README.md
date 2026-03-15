# Backend (Flask)

1. In backend folder, create your local env file:
   - `cp .env.example .env`
   - edit `.env` with MongoDB URI (find on MongoDB atlas)
2. Activate virtual environment
   - `source .venv/bin/activate`
3. Install packages:
   - `pip install flask flask-cors pymongo python-dotenv requests pyserial`
4. Start the backend API:
   - `python server.py`
5. In a second terminal, start mock sensor data:
   - `python mock_generator.py`
6. For real hardware data (instead of mock), run:
   - `python data_intake.py --port /dev/ttyACM0`
   - use the serial path for your RX LoRa module

API runs on `http://localhost:5001`
