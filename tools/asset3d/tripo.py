"""Minimal Tripo v3 API client (https://developers.tripo3d.ai) with a ledger.

- The key comes from $TRIPO_API_KEY and is never printed or stored.
- Every task is recorded in <out>/ledger.jsonl, keyed by a hash of its
  endpoint and body; a finished task with its files present is reused, so
  a rerun never pays twice. Uploads are cached by file hash the same way.
- Budget guard: a task is not started when the credits already consumed
  (ledger) plus its estimate would exceed max_credits.
- Output URLs expire after 5 minutes: files are downloaded as soon as the
  task succeeds.
- dry_run prints the requests and returns fake results, sending nothing.
"""
import hashlib
import json
import mimetypes
import os
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid

BASE = "https://openapi.tripo3d.ai/v3"
UA = "asset3d-pipeline/0.1"
TERMINAL = {"success", "failed", "cancelled", "banned", "expired"}


class BudgetExceeded(Exception):
    pass


class TaskFailed(Exception):
    pass


class Tripo:
    def __init__(self, out_dir, max_credits=800, dry_run=False, poll_secs=3.0):
        self.out_dir = out_dir
        self.ledger_path = os.path.join(out_dir, "ledger.jsonl")
        self.max_credits = max_credits
        self.dry_run = dry_run
        self.poll_secs = poll_secs
        self.key = os.environ.get("TRIPO_API_KEY", "")
        if not self.key and not dry_run:
            raise SystemExit("TRIPO_API_KEY is not set")
        os.makedirs(out_dir, exist_ok=True)

    # ---- ledger ---------------------------------------------------------
    def _entries(self):
        if not os.path.exists(self.ledger_path):
            return []
        with open(self.ledger_path, encoding="utf-8") as f:
            return [json.loads(l) for l in f if l.strip()]

    def _append(self, rec):
        rec["time"] = time.strftime("%Y-%m-%dT%H:%M:%S")
        with open(self.ledger_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(rec) + "\n")

    def credits_spent(self):
        spent = {}
        for e in self._entries():
            if e.get("kind") == "task" and e.get("task_id"):
                spent[e["task_id"]] = float(e.get("credits") or 0)
        return sum(spent.values())

    def _find(self, key):
        last = None
        for e in self._entries():
            if e.get("key") == key:
                last = e
        return last

    # ---- http -----------------------------------------------------------
    def _request(self, method, path, body=None, headers=None, data=None):
        url = BASE + path
        hdr = {"Authorization": "Bearer " + self.key, "User-Agent": UA}
        if body is not None:
            data = json.dumps(body).encode()
            hdr["Content-Type"] = "application/json"
        hdr.update(headers or {})
        for attempt in range(8):
            req = urllib.request.Request(url, data=data, method=method, headers=hdr)
            try:
                with urllib.request.urlopen(req, timeout=120) as r:
                    res = json.loads(r.read().decode())
            except urllib.error.HTTPError as e:
                txt = e.read().decode(errors="replace")
                if e.code == 429 or e.code >= 500:
                    wait = float(e.headers.get("Retry-After") or (5 * (attempt + 1)))
                    print("  http %d, retry in %.0fs: %s" % (e.code, wait, txt[:200]))
                    time.sleep(wait)
                    continue
                raise TaskFailed("%s %s -> HTTP %d: %s" % (method, path, e.code, txt[:500]))
            except urllib.error.URLError as e:
                print("  network error, retry: %s" % e)
                time.sleep(5 * (attempt + 1))
                continue
            if res.get("code") != 0:
                raise TaskFailed("%s %s -> %s" % (method, path, json.dumps(res)[:500]))
            return res["data"]
        raise TaskFailed("%s %s: gave up after retries" % (method, path))

    # ---- uploads --------------------------------------------------------
    def upload(self, path):
        raw = open(path, "rb").read()
        key = "upload:" + hashlib.sha256(raw).hexdigest()
        hit = self._find(key)
        if hit and hit.get("file_token"):
            return hit["file_token"]
        if self.dry_run:
            print("  [dry] upload %s" % path)
            return "dry_token_" + key[7:19]
        boundary = uuid.uuid4().hex
        ctype = mimetypes.guess_type(path)[0] or "application/octet-stream"
        name = os.path.basename(path)
        data = (("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                 "Content-Type: %s\r\n\r\n" % (boundary, name, ctype)).encode()
                + raw + ("\r\n--%s--\r\n" % boundary).encode())
        res = self._request("POST", "/files", data=data,
                            headers={"Content-Type": "multipart/form-data; boundary=" + boundary})
        token = res.get("file_token")
        self._append({"kind": "upload", "key": key, "file": path, "file_token": token})
        return token

    # ---- tasks ----------------------------------------------------------
    def run(self, endpoint, body, dest_dir, estimate, label=""):
        """Runs a task (or reuses a finished one) and downloads its outputs
        into dest_dir. Returns the ledger record."""
        key = "task:" + hashlib.sha256((endpoint + json.dumps(body, sort_keys=True)).encode()).hexdigest()
        hit = self._find(key)
        if hit and hit.get("status") == "success" and all(os.path.exists(p) for p in hit.get("files", {}).values()):
            print("  reuse %s (%s)" % (label or endpoint, hit["task_id"]))
            return hit
        spent = self.credits_spent()
        if spent + estimate > self.max_credits:
            raise BudgetExceeded("%s needs ~%d credits; spent %.0f of %d"
                                 % (label or endpoint, estimate, spent, self.max_credits))
        if self.dry_run:
            print("  [dry] POST %s %s (~%d credits)" % (endpoint, json.dumps(body), estimate))
            return {"task_id": "dry_" + key[5:17], "status": "success", "files": {}, "output": {}}
        task_id = None
        if hit and hit.get("status") in ("queued", "running") and hit.get("task_id"):
            task_id = hit["task_id"]
            print("  resume %s (%s)" % (label or endpoint, task_id))
        else:
            task_id = self._request("POST", endpoint, body=body)["task_id"]
            self._append({"kind": "task", "key": key, "label": label, "endpoint": endpoint,
                          "body": body, "task_id": task_id, "status": "queued"})
            print("  started %s: %s" % (label or endpoint, task_id))
        data = self.wait(task_id)
        rec = {"kind": "task", "key": key, "label": label, "endpoint": endpoint, "body": body,
               "task_id": task_id, "status": data.get("status"),
               "credits": data.get("credits_consumed"), "output": data.get("output") or {}}
        if data.get("status") != "success":
            rec["error"] = "%s %s" % (data.get("error_code"), data.get("error_message"))
            self._append(rec)
            raise TaskFailed("%s %s: %s" % (label or endpoint, data.get("status"), rec["error"]))
        rec["files"] = self._download_outputs(rec["output"], dest_dir)
        self._append(rec)
        print("  done %s: %s credits, %s" % (label or endpoint, rec["credits"], ", ".join(rec["files"])))
        return rec

    def wait(self, task_id, timeout=1800):
        t0 = time.time()
        last = None
        while time.time() - t0 < timeout:
            data = self._request("GET", "/tasks/" + task_id)
            st = data.get("status")
            if st in TERMINAL:
                return data
            if st not in ("queued", "running"):
                print("  unknown status %r for %s" % (st, task_id))
            prog = data.get("progress")
            if (st, prog) != last:
                print("    %s %s%%" % (st, prog))
                last = (st, prog)
            time.sleep(self.poll_secs)
        raise TaskFailed("timeout waiting for %s" % task_id)

    def _download_outputs(self, output, dest_dir):
        files = {}
        os.makedirs(dest_dir, exist_ok=True)

        def walk(obj, prefix):
            if isinstance(obj, dict):
                for k, v in obj.items():
                    walk(v, (prefix + "." if prefix else "") + k)
            elif isinstance(obj, list):
                for i, v in enumerate(obj):
                    walk(v, "%s.%d" % (prefix, i))
            elif isinstance(obj, str) and obj.startswith("http"):
                ext = os.path.splitext(urllib.parse.urlparse(obj).path)[1] or ".bin"
                name = prefix.replace("_url", "").replace(".", "_") + ext
                path = os.path.join(dest_dir, name)
                req = urllib.request.Request(obj, headers={"User-Agent": UA})
                with urllib.request.urlopen(req, timeout=300) as r, open(path, "wb") as f:
                    f.write(r.read())
                files[prefix] = path

        walk(output, "")
        return files
