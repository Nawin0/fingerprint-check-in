from fastapi import FastAPI, HTTPException
from datetime import datetime, timezone, timedelta
from fastapi.responses import JSONResponse
import mysql.connector

app = FastAPI()

mysql_config = {
    "host": "localhost",
    "user": "adfinger",
    "password": "FnWooxzhqWQsYJTD",
    "database": "fingerprint",
    "autocommit": True
}

async def check_in_mysql(fingerprint_check):
    query = "SELECT id, Name, idname FROM esp8266 WHERE id = %s"
    try:
        with mysql.connector.connect(**mysql_config) as conn:
            with conn.cursor() as cursor:
                cursor.execute(query, (fingerprint_check,))
                result = cursor.fetchone()

                if result:
                    id, name, idname = result
                    return True, name, idname
                else:
                    return False, None, None
    except mysql.connector.Error as e:
        print(f"Database Error: {e}")
        return False, None, None

@app.post("/fingerprint/{fingerprint_check}")
async def verify_fingerprint(fingerprint_check: int):
    is_valid, name, idname = await check_in_mysql(fingerprint_check)

    if is_valid:
        print("ID:", fingerprint_check)
        print("nisit id:", idname)
        print("name:", name)
        return response(idname, name)
    else:
        raise HTTPException(status_code=400, detail="Invalid fingerprint")

def response(idname, name):
    formatted_time = get_formatted_time()
    return JSONResponse(content={
        "user": name,
        "pin": idname,
        "timestamp": formatted_time
    })

def get_formatted_time():
    local_time = datetime.now(timezone(timedelta(hours=7)))
    return local_time.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]

async def add_to_mysql(fingerprint_id):
    query = "INSERT INTO esp8266 (id) VALUES (%s)"
    try:
        with mysql.connector.connect(**mysql_config) as conn:
            with conn.cursor() as cursor:
                cursor.execute(query, (fingerprint_id,))
                conn.commit()
                return True, fingerprint_id
    except mysql.connector.Error as e:
        print(f"Database Error: {e}")
        return False, None

@app.post("/enroll/{fingerprint_id}")
async def enroll_fingerprint(fingerprint_id: int):
    is_valid, enrolled_id = await add_to_mysql(fingerprint_id)

    if is_valid:
        print("ID:", enrolled_id)
        return response(enrolled_id, "Enrolled")
    else:
        raise HTTPException(status_code=400, detail="Invalid fingerprint")

async def delete_from_mysql(fingerprint_id):
    query = "DELETE FROM esp8266 WHERE id = %s"
    try:
        with mysql.connector.connect(**mysql_config) as conn:   
            with conn.cursor() as cursor:
                cursor.execute(query, (fingerprint_id,))
                conn.commit()
                return True, fingerprint_id
    except mysql.connector.Error as e:
        print(f"Database Error: {e}")
        return False, None

@app.post("/delete/{fingerprint_id}")
async def delete_fingerprint(fingerprint_id: int):
    is_valid, deleted_id = await delete_from_mysql(fingerprint_id)

    if is_valid:
        print("ID:", deleted_id)
        return {"message": f"Fingerprint with ID {deleted_id} deleted successfully"}
    else:
        raise HTTPException(status_code=400, detail="Invalid fingerprint")