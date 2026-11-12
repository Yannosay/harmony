import { defineEventHandler, getRequestURL, setResponseStatus } from 'h3';

export default defineEventHandler(async (event) => {
  const assets = event.context.cloudflare?.env?.ASSETS as { fetch: (input: URL) => Promise<Response> } | undefined;
  const url = getRequestURL(event);

  if (assets === undefined || url.pathname.includes('.'))
  {
    setResponseStatus(event, 404);
    return 'Not found';
  }

  return assets.fetch(new URL('/index.html', url));
});
