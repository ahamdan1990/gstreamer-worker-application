# Frontend Unified Layout Fix Guide

## Current Issues
1. ✅ **Layout inconsistency** - Different pages have different layouts
2. ✅ **Logs not real-time** - WebSocket reconnection implemented
3. ⏳ **Profile 500 error** - Needs backend investigation
4. ⏳ **Add/Edit forms broken** - Will be fixed with layout updates

## What We Fixed

### 1. Created Unified Layout Components
- **`components/layout.tsx`** - Main layout with sidebar
- **`components/sidebar.tsx`** - Updated with Logs tab + WebSocket status
- **`app/logs/page.tsx`** - Fixed with auto-reconnect WebSocket

### 2. Fixed CORS Configuration
- Updated `backend/.env` CORS_ORIGINS to comma-separated format
- Should fix cross-origin errors

## Quick Start - Testing

### 1. Restart Backend
```bash
cd backend
# Stop current backend (Ctrl+C)
uvicorn app.main:app --host 0.0.0.0 --port 8002 --reload
```

Watch for errors like:
```
sqlalchemy.exc.InvalidRequestError: ...
```

If you see this, note the full error and we'll fix the relationship.

### 2. Test Logs Page
```bash
# Open browser: http://localhost:3000/logs
# You should see:
#  - Sidebar on left with "Logs" tab
#  - Green "● Live" badge when connected
#  - Events appearing in real-time
```

### 3. Test Profile View
```bash
# Navigate to: http://localhost:3000/profiles
# Click "View" on any profile
# If you get 500 error, check backend console for stack trace
```

## Manual Page Fixes Needed

All pages need to be updated to use the unified layout. Here's the pattern:

### Before (Old Pattern):
```tsx
import { Header } from '@/components/header';

export default function MyPage() {
  return (
    <div>
      <Header title="My Page" />
      <main>
        {/* content */}
      </main>
    </div>
  );
}
```

### After (New Pattern):
```tsx
import { Layout, PageHeader } from '@/components/layout';

export default function MyPage() {
  return (
    <Layout>
      <PageHeader
        title="My Page"
        description="Optional description"
        action={<Button>Optional Action</Button>}
      />
      <div className="p-8">
        {/* content */}
      </div>
    </Layout>
  );
}
```

## Pages That Need Fixing

### High Priority (User mentioned broken):
1. ✅ `app/logs/page.tsx` - **FIXED**
2. ⏳ `app/profiles/[id]/page.tsx` - Profile view
3. ⏳ `app/profiles/[id]/edit/page.tsx` - Profile edit
4. ⏳ `app/profiles/add/page.tsx` - Profile add
5. ⏳ `app/webhooks/[id]/edit/page.tsx` - Webhook edit
6. ⏳ `app/webhooks/new/page.tsx` - Webhook add
7. ⏳ `app/watchlists/[id]/edit/page.tsx` - Watchlist edit
8. ⏳ `app/watchlists/new/page.tsx` - Watchlist add

### Medium Priority:
9. `app/page.tsx` - Dashboard
10. `app/cameras/page.tsx` - Camera list
11. `app/events/page.tsx` - Events list

### Lower Priority:
- All other pages in `app/` directory

## Debugging Profile 500 Error

When you restart the backend and try to view a profile, look for one of these errors:

### Possible Error 1: Missing Relationship
```
AttributeError: 'Profile' object has no attribute 'watchlists'
```
**Fix**: Check that both models import each other correctly

### Possible Error 2: Lazy Loading
```
sqlalchemy.orm.exc.DetachedInstanceError
```
**Fix**: Use `joinedload` in query (already done in code)

### Possible Error 3: JSON Serialization
```
TypeError: Object of type X is not JSON serializable
```
**Fix**: Check all fields being returned in the response

## Next Steps

1. **Test the logs page** - Verify WebSocket is working and events appear
2. **Check backend logs** - Look for the actual 500 error when viewing profile
3. **Report the error** - Share the exact error message
4. **Fix remaining pages** - I'll help update them one by one

## WebSocket Testing Checklist

Test if these events appear in logs page:
- [ ] `worker_started` - When backend starts
- [ ] `camera_started` - When camera starts
- [ ] `motion_detected` - When motion occurs
- [ ] `face_detected` - When face detected
- [ ] `person_recognized` - When person recognized
- [ ] `log` events - All log messages from worker

If NOT seeing events:
1. Check worker is running: `curl http://localhost:8081/health`
2. Check WebSocket is open: Browser console should show "Connected to worker WebSocket"
3. Check camera is running and detecting: Backend logs should show motion/face events
4. Try starting a camera if none are running

## Commands Reference

```bash
# Check worker health
curl http://localhost:8081/health

# Check cameras status
curl http://localhost:8081/api/cameras | jq

# Start a camera
curl -X POST http://localhost:8081/api/cameras/{camera_id}/start

# Test profile endpoint
curl http://localhost:8002/api/v1/profiles/ | jq
curl http://localhost:8002/api/v1/profiles/{profile_id} | jq
```

