#!/bin/bash
# Test profile endpoint to debug 500 error

echo "Testing profile endpoint..."
echo ""

# Get the first profile ID
echo "1. Fetching all profiles..."
PROFILE_ID=$(curl -s http://localhost:8002/api/v1/profiles/ | jq -r '.[0].id' 2>/dev/null)

if [ -z "$PROFILE_ID" ] || [ "$PROFILE_ID" = "null" ]; then
    echo "   No profiles found or error fetching profiles"
    echo ""
    echo "2. Full response:"
    curl -s http://localhost:8002/api/v1/profiles/ | jq '.'
    exit 1
fi

echo "   Found profile ID: $PROFILE_ID"
echo ""

# Try to get the profile details
echo "2. Fetching profile details for $PROFILE_ID..."
HTTP_CODE=$(curl -s -o /tmp/profile_response.json -w "%{http_code}" http://localhost:8002/api/v1/profiles/$PROFILE_ID)

echo "   HTTP Status: $HTTP_CODE"
echo ""

if [ "$HTTP_CODE" = "200" ]; then
    echo "   ✅ Success! Profile details:"
    cat /tmp/profile_response.json | jq '.'
else
    echo "   ❌ Error! Response:"
    cat /tmp/profile_response.json | jq '.'
fi

echo ""
echo "3. Check backend logs for detailed error information"
