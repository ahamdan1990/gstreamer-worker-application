"""add face detection and global settings

Revision ID: 9f3e5a8c2d41
Revises: 88480467c528
Create Date: 2025-11-27 10:00:00.000000

"""
from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

# revision identifiers, used by Alembic.
revision = '9f3e5a8c2d41'
down_revision = '88480467c528'
branch_labels = None
depends_on = None


def upgrade():
    # Add face detection configuration to cameras table
    op.add_column('cameras', sa.Column('face_detection_enabled', sa.Boolean(), nullable=True, server_default='false'))
    op.add_column('cameras', sa.Column('face_detection_config', postgresql.JSON(astext_type=sa.Text()), nullable=True))

    # Create global_settings table
    op.create_table(
        'global_settings',
        sa.Column('id', postgresql.UUID(as_uuid=True), nullable=False),
        sa.Column('setting_key', sa.String(length=255), nullable=False),
        sa.Column('setting_value', postgresql.JSON(astext_type=sa.Text()), nullable=False),
        sa.Column('description', sa.Text(), nullable=True),
        sa.Column('created_at', sa.DateTime(), nullable=True),
        sa.Column('updated_at', sa.DateTime(), nullable=True),
        sa.Column('updated_by', sa.String(length=255), nullable=True),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('setting_key')
    )
    op.create_index(op.f('ix_global_settings_setting_key'), 'global_settings', ['setting_key'], unique=True)

    # Insert default global settings
    # Person Tracking default configuration
    op.execute("""
        INSERT INTO global_settings (id, setting_key, setting_value, description, created_at, updated_at)
        VALUES (
            gen_random_uuid(),
            'person_tracking',
            '{"enabled": true, "presence_timeout_seconds": 300.0, "same_camera_cooldown_seconds": 10.0, "max_detections_per_person": 100, "enable_cross_camera_tracking": true, "enable_persistence": true, "persistence_path": "./person_tracking.json", "enable_daily_reset": true, "daily_reset_hour": 0, "archive_path": "./tracking_archives"}'::json,
            'Person tracking system configuration',
            NOW(),
            NOW()
        )
        ON CONFLICT (setting_key) DO NOTHING;
    """)

    # Manager default configuration
    op.execute("""
        INSERT INTO global_settings (id, setting_key, setting_value, description, created_at, updated_at)
        VALUES (
            gen_random_uuid(),
            'manager',
            '{"log_level": "INFO", "enable_metrics": true, "metrics_interval": 30.0}'::json,
            'Manager system configuration',
            NOW(),
            NOW()
        )
        ON CONFLICT (setting_key) DO NOTHING;
    """)


def downgrade():
    # Drop global_settings table
    op.drop_index(op.f('ix_global_settings_setting_key'), table_name='global_settings')
    op.drop_table('global_settings')

    # Remove face detection columns from cameras
    op.drop_column('cameras', 'face_detection_config')
    op.drop_column('cameras', 'face_detection_enabled')
