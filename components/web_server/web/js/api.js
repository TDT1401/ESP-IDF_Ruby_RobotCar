export async function post(path, body) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 350);
  try {
    const response = await fetch(path, {
      method:'POST',headers:{'X-Robot-Control':'1','Content-Type':'application/json'},
      body,signal:controller.signal,cache:'no-store'
    });
    if (!response.ok) {
      const error = new Error(String(response.status));
      error.status = response.status;
      throw error;
    }
    return await response.json();
  } finally { clearTimeout(timeout); }
}

export async function getStatus() {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(),500);
  try {
    const response = await fetch('/api/status',{cache:'no-store',signal:controller.signal});
    if (!response.ok) throw new Error(String(response.status));
    return await response.json();
  } finally { clearTimeout(timeout); }
}

// Page loss and session release must remain best effort.
export function postKeepalive(path, body) {
  return fetch(path, {
    method:'POST',headers:{'X-Robot-Control':'1','Content-Type':'application/json'},
    body,keepalive:true
  }).catch(() => {});
}
