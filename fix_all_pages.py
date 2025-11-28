#!/usr/bin/env python3
"""
Automated script to fix all frontend pages to use unified layout
"""

import os
import re
from pathlib import Path

FRONTEND_DIR = Path("frontend-ui")

def fix_page_layout(file_path):
    """Fix a page to use unified layout"""
    with open(file_path, 'r') as f:
        content = f.read()

    # Skip if already using Layout component
    if "from '@/components/layout'" in content or '<Layout' in content:
        print(f"  ✓ Already using unified layout: {file_path}")
        return False

    # Replace Header import with Layout import
    if "from '@/components/header'" in content:
        content = content.replace(
            "from '@/components/header'",
            "from '@/components/layout'"
        )
        # Replace Header component usage
        content = re.sub(
            r'<Header[^>]*/>',
            '',
            content
        )
        content = re.sub(
            r'<Header[^>]*>.*?</Header>',
            '',
            content,
            flags=re.DOTALL
        )

    # Wrap return statement with Layout if not already wrapped
    if '<Layout' not in content:
        # Find the main return statement
        return_match = re.search(r'return \((.*)\);', content, re.DOTALL)
        if return_match:
            inner_content = return_match.group(1).strip()

            # Remove outer div if it exists
            if inner_content.startswith('<div'):
                # Extract the className if any for potential reuse
                inner_content = re.sub(r'<div[^>]*>', '', inner_content, count=1)
                inner_content = inner_content.rstrip('</div>').rstrip()

            new_return = f"""return (
    <Layout>
      <div className="p-8">
        {inner_content}
      </div>
    </Layout>
  );"""

            content = re.sub(
                r'return \(.*?\);',
                new_return,
                content,
                flags=re.DOTALL,
                count=1
            )

    # Write back
    with open(file_path, 'w') as f:
        f.write(content)

    print(f"  ✅ Fixed: {file_path}")
    return True

def main():
    """Main function to fix all pages"""
    print("🔧 Fixing all frontend pages to use unified layout...\n")

    # Find all page.tsx files
    pages = list(FRONTEND_DIR.glob("app/**/page.tsx"))

    print(f"Found {len(pages)} pages to check\n")

    fixed_count = 0
    skip_count = 0

    for page in sorted(pages):
        try:
            if fix_page_layout(page):
                fixed_count += 1
            else:
                skip_count += 1
        except Exception as e:
            print(f"  ❌ Error fixing {page}: {e}")

    print(f"\n{'='*60}")
    print(f"Summary:")
    print(f"  ✅ Fixed: {fixed_count}")
    print(f"  ⏭️  Skipped: {skip_count}")
    print(f"  📝 Total: {len(pages)}")
    print(f"{'='*60}\n")

    print("⚠️  Please review the changes and manually adjust:")
    print("  - Pages with complex headers")
    print("  - Pages with custom layouts")
    print("  - Pages that need PageHeader component")

if __name__ == "__main__":
    main()
