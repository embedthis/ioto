/*
    auth.js - Show the login state on each page

    An external script so the pages run under the Content-Security-Policy "default-src 'self'", which
    refuses inline scripts. Server data is written only with textContent, never as HTML.
 */

async function showAuth() {
    let auth = null
    try {
        let response = await fetch('/api/public/getAuth')
        if (response.ok) {
            auth = await response.json()
        }
    } catch (err) {
        auth = null
    }
    let notes = document.getElementById('notes')
    let link = document.getElementById('username')
    let log = document.getElementById('log')
    let button = document.getElementById('logbutton')

    if (!auth) {
        if (notes) {
            notes.textContent = 'ERROR: App not built correctly.'
        }
        if (log) {
            log.style.display = 'none'
        }
        return
    }
    let loggedIn = Boolean(auth.username)
    if (link) {
        link.textContent = loggedIn ? auth.username : 'Login'
        link.href = loggedIn ? '/public/logout.html' : '/public/login.html'
    }
    if (notes) {
        notes.textContent = loggedIn ? `Logged in as "${auth.username}"` : 'Not logged in'
    }
    if (log && button) {
        button.textContent = loggedIn ? 'Log Out' : 'Log In'
        log.href = loggedIn ? '/public/logout.html' : '/public/login.html'
    }
}

showAuth()
