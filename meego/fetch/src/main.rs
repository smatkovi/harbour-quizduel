//! quizduel-holer: ein davinci-Request für die MeeGo-Ausgabe von harbour-quizduel.
//!
//! Harmattans Qt 4 sitzt auf OpenSSL 0.9.8 und kann das magbackend (TLS 1.2+)
//! nicht erreichen — deshalb dieser statische musl-Helfer mit rustls (wie
//! passviewer-fetch). Er spricht das VOLLE davinci-Protokoll, nicht nur ein GET:
//! er baut den Request<T>-Umschlag, signiert (ticket), komprimiert, verschlüsselt,
//! sendet und entschlüsselt die Antwort — exakt wie src/davinciprotocol.cpp.
//!
//! Betrieb: eine Request-Beschreibung als JSON über stdin, Antwort als JSON auf
//! stdout. Der Qt-4-Teil (NetBridge) ruft ihn per QProcess.
//!
//!   stdin : {"method":"login","deviceId":"<32hex>","session":"","userId":0,
//!            "locale":"de_DE","timeZone":"Europe/Vienna","localOffsetMs":7200000,
//!            "fields":{"username":"u","password":"p","quizDuelLocale":"de_DE"}}
//!   stdout: {"status":200,"payload":<entschlüsseltes inneres JSON>}   bei Erfolg
//!           {"error":"..."}                                           sonst
//!
//! Exit-Code immer 0, sobald die Eingabe stimmt; der Aufrufer liest das JSON.
//! `--selftest <krypto-testvektor.json>` prüft die Krypto offline (kein Netz).

use std::io::{Read, Write};
use std::time::Duration;

use cbc::cipher::{block_padding::Pkcs7, BlockDecryptMut, BlockEncryptMut, KeyIvInit};
use sha1::{Digest, Sha1};
use serde_json::{json, Map, Value};

type Enc = cbc::Encryptor<aes::Aes256>;
type Dec = cbc::Decryptor<aes::Aes256>;

// Konstanten — Herkunft BEFUND.md "★ KRYPTO GELÖST" / "★★ ticket GELÖST",
// identisch zu src/davinciprotocol.{h,cpp}.
const KEY_HEX: &str = "REDIGIERT_ServerSharedKeyV2_64hex_aus_der_App";
const APP_KEY: &str = "REDIGIERT_applicationKey";
const SALT: &str = "REDIGIERT_ticketSalt";
const VERSION: &str = "1.70.04";
const UA: &str = "Android/New QuizDuel/1.70.04";
const UNITY: &str = "6000.3.22f1";
const API_ROOT: &str = "https://api.prod.magbackend.se/api/";
const QD_BASE: &str = "https://api.prod.magbackend.se/api/user/quizduel/";
const BOM: [u8; 3] = [0xEF, 0xBB, 0xBF];

fn key() -> Vec<u8> {
    hex::decode(KEY_HEX).expect("key hex")
}

fn gzip(data: &[u8]) -> Vec<u8> {
    let mut e = flate2::write::GzEncoder::new(Vec::new(), flate2::Compression::default());
    e.write_all(data).expect("gzip write");
    e.finish().expect("gzip finish")
}

fn gunzip(data: &[u8]) -> Option<Vec<u8>> {
    let mut d = flate2::read::GzDecoder::new(data);
    let mut o = Vec::new();
    d.read_to_end(&mut o).ok().map(|_| o)
}

fn aes_enc(plain: &[u8], iv: &[u8]) -> Vec<u8> {
    Enc::new_from_slices(&key(), iv)
        .expect("aes key/iv")
        .encrypt_padded_vec_mut::<Pkcs7>(plain)
}

fn aes_dec(ct: &[u8], iv: &[u8]) -> Option<Vec<u8>> {
    Dec::new_from_slices(&key(), iv)
        .ok()?
        .decrypt_padded_vec_mut::<Pkcs7>(ct)
        .ok()
}

/// ticket = kleingeschriebenes Hex von SHA1(UTF-8( SALT + version +
/// methodLastSegment + session + epochMs )). session = "" vor dem Login.
fn ticket(method: &str, session: &str, epoch_ms: &str) -> String {
    let last = method.rsplit('/').next().unwrap_or(method);
    let s = format!("{}{}{}{}{}", SALT, VERSION, last, session, epoch_ms);
    let mut h = Sha1::new();
    h.update(s.as_bytes());
    hex::encode(h.finalize())
}

fn now_ms() -> u128 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .expect("clock")
        .as_millis()
}

fn str_field<'a>(req: &'a Value, k: &str, dflt: &'a str) -> &'a str {
    req.get(k).and_then(|v| v.as_str()).unwrap_or(dflt)
}

/// Spiegelt davinci::baseEnvelope + die methodenspezifischen Felder.
fn build_envelope(req: &Value, ts: &str, tkt: &str) -> Value {
    let session = str_field(req, "session", "");
    let session_val = if session.is_empty() { Value::Null } else { json!(session) };
    let user_id = req.get("userId").and_then(|v| v.as_i64()).unwrap_or(0);
    let off = req.get("localOffsetMs").and_then(|v| v.as_i64()).unwrap_or(0);
    let local_ts = (ts.parse::<i64>().unwrap_or(0) + off).to_string();

    let mut o = Map::new();
    o.insert("userId".into(), json!(user_id));
    o.insert("applicationId".into(), json!(0));
    o.insert("applicationKey".into(), json!(APP_KEY));
    o.insert("timestamp".into(), json!(ts));
    o.insert("localTimestamp".into(), json!(local_ts));
    o.insert("localTimeZone".into(), json!(str_field(req, "timeZone", "Europe/Vienna")));
    o.insert("version".into(), json!(VERSION));
    o.insert("session".into(), session_val);
    o.insert("ticket".into(), json!(tkt));
    o.insert("platform".into(), json!("android"));
    o.insert("locale".into(), json!(str_field(req, "locale", "de_DE")));
    o.insert("premium".into(), json!(false));
    o.insert("deviceModel".into(), json!(str_field(req, "deviceModel", "Nokia N9 (MeeGo Harmattan)")));
    o.insert("osVersion".into(), json!(str_field(req, "osVersion", "MeeGo 1.2 Harmattan")));
    o.insert("advertisingId".into(), json!(""));
    o.insert("deviceId".into(), json!(str_field(req, "deviceId", "")));
    o.insert("clientGeneratedId".into(), Value::Null);
    o.insert("attributionProviderId".into(), Value::Null);
    o.insert(
        "applicationActivityStatus".into(),
        json!({"activityId":0,"presenceBucketUpdates":null}),
    );
    o.insert("tracks".into(), Value::Null);

    if let Some(fields) = req.get("fields").and_then(|v| v.as_object()) {
        for (k, v) in fields {
            o.insert(k.clone(), v.clone());
        }
    }
    Value::Object(o)
}

/// Antwort entschlüsseln: (opt. HTTP-gzip) -> AES mit demselben IV -> inneres gzip
/// -> UTF-8-JSON ohne BOM. reqwest zieht die Transport-gzip-Schicht selbst ab; die
/// 1f8b-Erkennung ist nur Rückfall (Chiffrat beginnt praktisch nie mit 1f 8b).
fn decode_response(body: &[u8], iv: &[u8]) -> Option<String> {
    let mut blob = body.to_vec();
    if blob.len() >= 2 && blob[0] == 0x1f && blob[1] == 0x8b {
        if let Some(u) = gunzip(&blob) {
            blob = u;
        }
    }
    let gz = aes_dec(&blob, iv)?;
    let mut json = gunzip(&gz)?;
    if json.starts_with(&BOM) {
        json.drain(0..3);
    }
    String::from_utf8(json).ok()
}

fn do_post(url: &str, body: Vec<u8>, iv_hex: &str) -> Result<(u16, Vec<u8>), String> {
    let client = reqwest::blocking::Client::builder()
        .timeout(Duration::from_secs(40))
        .build()
        .map_err(|e| e.to_string())?;
    let resp = client
        .post(url)
        .header("content-type", "application/json")
        .header("user-agent", UA)
        .header("request-version", "2")
        .header("x-davinci-gzip-compression", "True")
        .header("x-unity-version", UNITY)
        .header("payload-session", iv_hex)
        .body(body)
        .send()
        .map_err(|e| e.to_string())?;
    let status = resp.status().as_u16();
    // Mit dem gzip-Feature liefert bytes() bereits die transport-entpackte Nutzlast.
    let bytes = resp.bytes().map_err(|e| e.to_string())?.to_vec();
    Ok((status, bytes))
}

fn emit(v: Value) {
    let mut out = std::io::stdout();
    let _ = writeln!(out, "{}", v);
}

// ===========================================================================
// Zuglogik + Raum-Aufbereitung — PORTIERT 1:1 aus src/netclient.cpp (parseRoom +
// dv*-Helfer). Referenz ist die C++-Fassung (per Live-Spiel auf SFOS bestätigt):
// 6 Runden, ALTERNIERENDER Wähler, aktuelle Runde = niedrigste, die NICHT beide
// beantwortet haben. participantId ist RAUM-LOKAL (nicht = userId). Weicht die
// MeeGo-Anzeige von SFOS ab, ist DIESE Rust-Fassung falsch, nicht der Server.
// QtQuick-1.1-JS darf diese Rechnerei NICHT machen (V4-Stacküberlauf wie auf SFOS),
// Qt-4-C++ kann kein JSON — darum hier im Holer (serde_json).

fn as_list(v: Option<&Value>) -> Vec<Value> {
    match v {
        Some(Value::Array(a)) => a.clone(),
        Some(Value::Object(_)) => vec![v.unwrap().clone()], // SingleOrArray
        _ => Vec::new(),
    }
}

// davinci serialisiert Zahlen oft als STRING ("userId":"35242598004"). as_i64() liefert
// dafür None -> Teilnehmer-Abgleich scheitert und der Gegner wird falsch (eigener Name).
// num() akzeptiert Zahl UND Zahl-als-String. (SFOS trifft das nicht: QVariant::toLongLong
// parst Strings; die Invocations tragen participantUserId als echte Zahl, darum lief die
// Zuglogik trotzdem — nur die Teilnehmer-userId kommt als String.)
fn num(v: Option<&Value>) -> Option<i64> {
    match v {
        Some(Value::Number(n)) => n.as_i64(),
        Some(Value::String(s)) => s.parse::<i64>().ok(),
        _ => None,
    }
}

fn invocations(room: &Value) -> Vec<Value> {
    room.get("gameState")
        .and_then(|g| g.get("json"))
        .and_then(|j| j.get("commandInvocationStack"))
        .and_then(|c| c.get("invocations"))
        .and_then(|i| i.as_array())
        .cloned()
        .unwrap_or_default()
}

fn dp_round(m: &Value) -> Option<i64> {
    num(m.get("dictionaryPayload").and_then(|d| d.get("roundIndex")))
}

fn who_selected(inv: &[Value], r: i64) -> i64 {
    for m in inv {
        if m.get("command").and_then(|v| v.as_str()) != Some("select_category") { continue; }
        if dp_round(m) == Some(r) {
            return num(m.get("participantUserId")).unwrap_or(0);
        }
    }
    0
}

fn cat_selected(inv: &[Value], r: i64) -> i64 {
    for m in inv {
        if m.get("command").and_then(|v| v.as_str()) != Some("select_category") { continue; }
        if dp_round(m) == Some(r) {
            return num(m.get("dictionaryPayload").and_then(|d| d.get("categoryId"))).unwrap_or(-1);
        }
    }
    -1
}

// equal_me: true -> Einreichung VON mir, false -> von jemand ANDEREM.
fn submitted(inv: &[Value], r: i64, my: i64, equal_me: bool) -> bool {
    for m in inv {
        if m.get("command").and_then(|v| v.as_str()) != Some("submit_answers") { continue; }
        let is_me = num(m.get("participantUserId")).unwrap_or(0) == my;
        if equal_me != is_me { continue; }
        if dp_round(m) == Some(r) { return true; }
    }
    false
}

// Wähler von Runde 0 = erster Beitritt (join_game, kleinster Zeitstempel) = Initiator.
fn first_chooser(inv: &[Value]) -> i64 {
    let mut best = 0i64;
    let mut best_t = -1i64;
    for m in inv {
        if m.get("command").and_then(|v| v.as_str()) != Some("join_game") { continue; }
        let t = num(m.get("utcTimeInMilliSeconds")).unwrap_or(0);
        if best_t < 0 || t < best_t {
            best_t = t;
            best = num(m.get("participantUserId")).unwrap_or(0);
        }
    }
    best
}

fn mine_to_select(inv: &[Value], r: i64, my: i64) -> bool {
    let mut r0 = -1i64;
    let mut c0me = false;
    for k in 0..6 {
        let w = who_selected(inv, k);
        if w != 0 { r0 = k; c0me = w == my; break; }
    }
    if r0 < 0 {
        // Noch keine Kategorie gewählt → Runde 0 wählt der Initiator, danach abwechselnd.
        let fc = first_chooser(inv);
        let me_first = if fc != 0 { fc == my } else { true };
        return if r % 2 == 0 { me_first } else { !me_first };
    }
    if (r - r0) % 2 == 0 { c0me } else { !c0me }
}

fn next_playable_round(inv: &[Value], my: i64) -> i64 {
    let mut cur = 6i64;
    for r in 0..6 {
        if !(submitted(inv, r, my, true) && submitted(inv, r, my, false)) { cur = r; break; }
    }
    if cur >= 6 { return -1; }
    if submitted(inv, cur, my, true) { return -1; }     // meine Runde erledigt -> warten
    if cat_selected(inv, cur) >= 0 { return cur; }       // Kategorie steht -> antworten
    if mine_to_select(inv, cur, my) { return cur; }      // ich wähle + antworte
    -1
}

fn participants(room: &Value) -> Vec<Value> {
    as_list(room.get("participants"))
}

fn my_participant_id(room: &Value, my: i64) -> i64 {
    for p in participants(room) {
        if num(p.get("userId")) == Some(my) {
            let pid = num(p.get("participantId")).unwrap_or(0);
            if pid != 0 { return pid; }
        }
    }
    for m in invocations(room) {
        if num(m.get("participantUserId")) == Some(my) {
            let pid = num(m.get("participantId")).unwrap_or(0);
            if pid != 0 { return pid; }
        }
    }
    0
}

fn opponent_entry(room: &Value, my: i64) -> Option<Value> {
    for p in participants(room) {
        if num(p.get("userId")) != Some(my) { return Some(p); }
    }
    None
}

fn opponent_name(opp: &Option<Value>) -> String {
    if let Some(o) = opp {
        let u = o.get("username").and_then(|v| v.as_str()).unwrap_or("");
        if !u.is_empty() { return u.to_string(); }
        let t = o.get("usernameForTag").and_then(|v| v.as_str()).unwrap_or("");
        if !t.is_empty() { return t.to_string(); }
    }
    "…".to_string()
}

// Macht aus gameState.json (JSON-STRING) ein Objekt und hängt die flachen Zug-Felder an.
fn enrich_room(room: &mut Value, my: i64) {
    if let Some(obj) = room.as_object_mut() {
        // gameState.json (String) -> Objekt
        if let Some(gs) = obj.get_mut("gameState") {
            if let Some(gsobj) = gs.as_object_mut() {
                let parsed = gsobj.get("json").and_then(|j| j.as_str())
                    .and_then(|s| serde_json::from_str::<Value>(s).ok());
                if let Some(p) = parsed { gsobj.insert("json".into(), p); }
            }
        }
    }
    let finished = room.get("roomStatus").and_then(|v| v.as_str()) == Some("finished");
    let inv = invocations(room);
    let tr = if finished { -2 } else { next_playable_round(&inv, my) };
    let turn_status = if finished { "finished" } else if tr >= 0 { "myturn" } else { "waiting" };
    let sel_cat = if tr >= 0 { cat_selected(&inv, tr) } else { -1 };
    let opp = opponent_entry(room, my);
    // Punktestand: richtige Antworten (answer==0) je Spieler.
    let (mut my_score, mut opp_score) = (0i64, 0i64);
    for m in &inv {
        if m.get("command").and_then(|v| v.as_str()) != Some("submit_answers") { continue; }
        let is_me = num(m.get("participantUserId")).unwrap_or(0) == my;
        if let Some(ans) = m.get("dictionaryPayload").and_then(|d| d.get("answers")).and_then(|a| a.as_array()) {
            for a in ans {
                if num(a.get("answer")) == Some(0) {
                    if is_me { my_score += 1; } else { opp_score += 1; }
                }
            }
        }
    }
    let outcome = if !finished { String::new() }
        else if my_score > opp_score { "won".into() }
        else if my_score < opp_score { "lost".into() }
        else { "draw".into() };
    let my_pid = my_participant_id(room, my);
    if let Some(obj) = room.as_object_mut() {
        obj.insert("turnRound".into(), json!(tr));
        obj.insert("turnStatus".into(), json!(turn_status));
        obj.insert("selCatId".into(), json!(sel_cat));
        obj.insert("opponent".into(), json!(opponent_name(&opp)));
        // oppUserId als ZAHL ausgeben (nicht den rohen String) — QML vergleicht > 0.
        obj.insert("oppUserId".into(), json!(num(opp.as_ref().and_then(|o| o.get("userId"))).unwrap_or(0)));
        obj.insert("oppAvatar".into(), json!(opp.as_ref().and_then(|o| o.get("avatarUrl")).and_then(|v| v.as_str()).unwrap_or("")));
        obj.insert("myScore".into(), json!(my_score));
        obj.insert("oppScore".into(), json!(opp_score));
        obj.insert("outcome".into(), json!(outcome));
        obj.insert("myParticipantId".into(), json!(my_pid));
    }
}

// Schlanke Raumzeile für die Liste (ohne gameState -> die HomePage kaut nichts Großes).
fn slim_room(room: &Value) -> Value {
    let keys = ["id", "roomStatus", "turnRound", "turnStatus", "selCatId", "opponent",
                "oppUserId", "oppAvatar", "myScore", "oppScore", "outcome", "myParticipantId"];
    let mut o = Map::new();
    for k in keys { if let Some(v) = room.get(k) { o.insert(k.into(), v.clone()); } }
    o.insert("status".into(), json!("OK"));
    Value::Object(o)
}

// Vollen (angereicherten) Raum als <cacheDir>/<id>.json ablegen — die Brücke liest ihn
// bei openRoom direkt (ohne Holer-Aufruf, ohne JSON-Parsen in Qt 4).
fn write_room_cache(cache_dir: &str, room: &Value) {
    if cache_dir.is_empty() { return; }
    let id = room.get("id").and_then(|v| v.as_str()).unwrap_or("");
    if id.is_empty() { return; }
    let _ = std::fs::create_dir_all(cache_dir);
    let path = format!("{}/{}.json", cache_dir, id);
    if let Ok(s) = serde_json::to_string(room) { let _ = std::fs::write(path, s); }
}

// Nach dem Entschlüsseln: raumtragende Antworten anreichern + cachen. Refresh liefert
// die SCHLANKE Liste zurück, Einzelraum-Antworten (Match/Zug) den vollen Raum.
fn postprocess(method: &str, mut payload: Value, my: i64, cache_dir: &str) -> Value {
    let enrich_each = |arr: &mut Vec<Value>| {
        for r in arr.iter_mut() { enrich_room(r, my); write_room_cache(cache_dir, r); }
    };
    if method == "multiplayer/refresh" {
        if let Some(rooms) = payload.get_mut("rooms") {
            let mut list = as_list(Some(rooms));
            enrich_each(&mut list);
            let slim: Vec<Value> = list.iter().map(slim_room).collect();
            *rooms = Value::Array(slim);
        }
        return payload;
    }
    // Match/Zug: genau EIN Raum, voll zurück. Feldname je nach Antwort.
    for key in ["room", "updatedRooms"] {
        if let Some(v) = payload.get(key).cloned() {
            let mut one = match v { Value::Array(a) => a.into_iter().next().unwrap_or(Value::Null), other => other };
            if one.is_object() {
                enrich_room(&mut one, my);
                write_room_cache(cache_dir, &one);
                if let Some(obj) = payload.as_object_mut() { obj.insert(key.into(), one); }
            }
        }
    }
    payload
}

// Bildformat aus den Magic-Bytes — die CDN-URLs haben KEINE Endung (…/40_xhdpi_8ca7…),
// und Harmattans Qt-4-Bildleser erkennt das Format ohne Endung nicht zuverlässig.
fn img_ext(b: &[u8]) -> &'static str {
    if b.len() >= 8 && &b[0..8] == b"\x89PNG\r\n\x1a\n" { "png" }
    else if b.len() >= 3 && &b[0..3] == b"\xff\xd8\xff" { "jpg" }
    else if b.len() >= 6 && (&b[0..6] == b"GIF87a" || &b[0..6] == b"GIF89a") { "gif" }
    else if b.len() >= 12 && &b[0..4] == b"RIFF" && &b[8..12] == b"WEBP" { "webp" }
    else { "" }
}

// --fetch <url> <outBasis>: Bild über rustls (TLS 1.2) holen, mit ERKANNTER Endung ablegen
// (outBasis + ".png"/".jpg"/…) und den TATSÄCHLICHEN Pfad zurückgeben. Harmattans Qt 4 /
// OpenSSL 0.9.8 kann die CDN-https-Bilder NICHT selbst laden.
fn fetch_file(url: &str, out_base: &str) -> i32 {
    let client = match reqwest::blocking::Client::builder().timeout(Duration::from_secs(40)).build() {
        Ok(c) => c, Err(e) => { emit(json!({"error": e.to_string()})); return 1; }
    };
    match client.get(url).header("user-agent", UA).send() {
        Ok(resp) => {
            let status = resp.status().as_u16();
            match resp.bytes() {
                Ok(b) => {
                    let ext = img_ext(&b);
                    // Nur ein ECHTES Bild (2xx + erkannte Magic-Bytes) ergibt einen path.
                    // 404/HTML/Nicht-Bild -> KEIN path -> die Brücke zeigt kein (kaputtes) Bild.
                    if !(200..300).contains(&status) || ext.is_empty() {
                        emit(json!({"status": status, "error": "not an image"}));
                        return 1;
                    }
                    if let Some(parent) = std::path::Path::new(out_base).parent() { let _ = std::fs::create_dir_all(parent); }
                    let out = format!("{}.{}", out_base, ext);
                    match std::fs::write(&out, &b) {
                        Ok(_) => { emit(json!({"status": status, "path": out})); 0 }
                        Err(e) => { emit(json!({"error": e.to_string()})); 1 }
                    }
                }
                Err(e) => { emit(json!({"error": e.to_string()})); 1 }
            }
        }
        Err(e) => { emit(json!({"error": e.to_string()})); 1 }
    }
}

fn run() {
    let mut input = String::new();
    if std::io::stdin().read_to_string(&mut input).is_err() {
        emit(json!({"error":"cannot read stdin"}));
        return;
    }
    let req: Value = match serde_json::from_str(&input) {
        Ok(v) => v,
        Err(e) => {
            emit(json!({ "error": format!("bad request json: {}", e) }));
            return;
        }
    };
    let method = str_field(&req, "method", "");
    if method.is_empty() {
        emit(json!({"error":"no method"}));
        return;
    }
    let ts = now_ms().to_string();
    let session = str_field(&req, "session", "");
    let tkt = ticket(method, session, &ts);
    let env = build_envelope(&req, &ts, &tkt);
    let json_bytes = serde_json::to_vec(&env).expect("serialize");
    let mut payload = BOM.to_vec();
    payload.extend_from_slice(&json_bytes);
    let gz = gzip(&payload);

    let mut iv = [0u8; 16];
    if getrandom::getrandom(&mut iv).is_err() {
        emit(json!({"error":"no randomness"}));
        return;
    }
    let ct = aes_enc(&gz, &iv);
    let iv_hex = hex::encode_upper(iv);
    // Methode MIT '/' (chatthread/..., inbox/..., boards/...) hängt an /api/,
    // kurze (login, getFriends) an /api/user/quizduel/.
    let url = if method.contains('/') {
        format!("{}{}", API_ROOT, method)
    } else {
        format!("{}{}", QD_BASE, method)
    };

    let user_id = req.get("userId").and_then(|v| v.as_i64()).unwrap_or(0);
    let cache_dir = str_field(&req, "cacheDir", "").to_string();

    match do_post(&url, ct, &iv_hex) {
        Ok((status, body)) => match decode_response(&body, &iv) {
            Some(txt) => {
                let payload: Value = serde_json::from_str(&txt).unwrap_or(Value::String(txt));
                let payload = postprocess(method, payload, user_id, &cache_dir);
                emit(json!({ "status": status, "payload": payload }));
            }
            None => emit(json!({ "error": format!("response decode failed (http {})", status) })),
        },
        Err(e) => emit(json!({ "error": e })),
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.get(1).map(|s| s.as_str()) == Some("--selftest") {
        let path = args.get(2).map(|s| s.as_str()).unwrap_or("krypto-testvektor.json");
        std::process::exit(selftest(path));
    }
    // Nur zur Fehlersuche: eine (bereits entschlüsselte) refresh-Antwort anreichern/schlank
    // machen, wie postprocess es täte. quizduel-holer --enrich-test <datei.json> <userId>
    if args.get(1).map(|s| s.as_str()) == Some("--enrich-test") {
        let file = args.get(2).cloned().unwrap_or_default();
        let uid = args.get(3).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0);
        let txt = std::fs::read_to_string(&file).expect("read file");
        let payload: Value = serde_json::from_str(&txt).expect("parse json");
        let out = postprocess("multiplayer/refresh", payload, uid, "/tmp/enrichtest");
        println!("{}", serde_json::to_string(&out).unwrap());
        return;
    }
    if args.get(1).map(|s| s.as_str()) == Some("--fetch") {
        let url = args.get(2).map(|s| s.as_str()).unwrap_or("");
        let out = args.get(3).map(|s| s.as_str()).unwrap_or("");
        if url.is_empty() || out.is_empty() {
            emit(json!({"error":"--fetch <url> <outpath>"}));
            std::process::exit(1);
        }
        std::process::exit(fetch_file(url, out));
    }
    run();
}

// ---------------------------------------------------------------------------
// Offline-KAT gegen recon/krypto-testvektor.json — prüft ticket, AES (beide
// Richtungen, derselbe IV), gzip/BOM und den 3-Schichten-Antwort-Pfad, ohne Netz.
fn selftest(path: &str) -> i32 {
    let raw = match std::fs::read_to_string(path) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("FAIL: kann {} nicht lesen: {}", path, e);
            return 1;
        }
    };
    let v: Value = serde_json::from_str(&raw).expect("vector json");
    let iv = hex::decode(v["iv_hex"].as_str().unwrap()).unwrap();
    let mut ok = true;
    let mut check = |name: &str, cond: bool| {
        println!("[{}] {}", if cond { "PASS" } else { "FAIL" }, name);
        ok &= cond;
    };

    // 1) ticket-Formel gegen den im Umschlag beglaubigten Wert.
    let req_json: Value = serde_json::from_str(v["request"]["plaintext_json"].as_str().unwrap()).unwrap();
    let want_ticket = req_json["ticket"].as_str().unwrap();
    let got_ticket = ticket("getUsersByDevice", "", req_json["timestamp"].as_str().unwrap());
    check("ticket SHA1(salt+ver+method+session+ts)", got_ticket == want_ticket);

    // 2) Request entschlüsseln: AES(ct,iv) -> 1f8b -> gunzip -> BOM+json.
    let ct = hex::decode(v["request"]["ciphertext_hex"].as_str().unwrap()).unwrap();
    let dec = aes_dec(&ct, &iv);
    let mut req_plain = dec.clone().unwrap_or_default();
    let req_starts_gzip = req_plain.first() == Some(&0x1f) && req_plain.get(1) == Some(&0x8b);
    check("request AES-decrypt -> gzip-magic", req_starts_gzip);
    let req_unzipped = gunzip(&req_plain).unwrap_or_default();
    let req_bom = req_unzipped.starts_with(&BOM);
    check("request inner gzip -> UTF-8-BOM", req_bom);
    let req_text = String::from_utf8_lossy(if req_bom { &req_unzipped[3..] } else { &req_unzipped });
    check("request JSON == Vektor", req_text == v["request"]["plaintext_json"].as_str().unwrap());

    // 3) Antwort: wire -> (HTTP gunzip) -> AES(iv) -> gunzip -> "null".
    let wire = hex::decode(v["response"]["wire_hex"].as_str().unwrap()).unwrap();
    let resp_txt = decode_response(&wire, &iv);
    check("response 3-Schichten-Decode == \"null\"", resp_txt.as_deref() == Some(v["response"]["plaintext_json"].as_str().unwrap()));

    // 4) Roundtrip: AES/gzip sind invers (wir können die Byte-genaue Chiffre nicht
    //    reproduzieren — gzip ist nicht deterministisch —, aber die Umkehr muss stimmen).
    let sample = b"\xef\xbb\xbf{\"x\":1}";
    let round = aes_enc(&gzip(sample), &iv);
    let back = aes_dec(&round, &iv).and_then(|g| gunzip(&g));
    check("roundtrip gzip+AES invers", back.as_deref() == Some(&sample[..]));
    // Und: unser frisch erzeugtes Chiffrat entschlüsselt wieder zum Umschlag.
    req_plain.clear();

    if ok {
        println!("== KAT OK ==");
        0
    } else {
        println!("== KAT FEHLGESCHLAGEN ==");
        1
    }
}
