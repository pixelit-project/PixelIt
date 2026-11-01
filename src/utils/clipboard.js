// The device WebUI uses HTTP, where navigator.clipboard may be unavailable.
export async function copyToClipboard(value) {
    const text = String(value);
    if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
        try {
            await navigator.clipboard.writeText(text);
            return;
        } catch (error) {
            // Try the user-initiated legacy copy path if clipboard permissions reject the request.
        }
    }

    const previousFocus = document.activeElement;
    const input = document.createElement('textarea');
    input.value = text;
    input.setAttribute('readonly', '');
    input.style.position = 'fixed';
    input.style.top = '0';
    input.style.left = '0';
    input.style.opacity = '0';
    input.style.fontSize = '16px';
    document.body.appendChild(input);
    try {
        input.focus({ preventScroll: true });
        input.select();
        input.setSelectionRange(0, text.length);
        if (typeof document.execCommand !== 'function' || !document.execCommand('copy')) {
            throw new Error('Clipboard access is unavailable.');
        }
    } finally {
        document.body.removeChild(input);
        if (previousFocus && typeof previousFocus.focus === 'function') {
            previousFocus.focus({ preventScroll: true });
        }
    }
}
