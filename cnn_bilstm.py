"""
CNN+BiLSTM 故障诊断模型 — TensorFlow/Keras 实现
支持 CLI 调用: 训练模式 / 推理模式

用法:
  python cnn_bilstm.py --mode train --data <xlsx_path> [--output_dir <dir>]
  python cnn_bilstm.py --mode infer --features <comma_separated> --model_dir <dir>
"""

import numpy as np
import pandas as pd
import sys
import os
import json
import argparse
import warnings
warnings.filterwarnings('ignore')

os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

from sklearn.preprocessing import MinMaxScaler
import tensorflow as tf
from tensorflow.keras import layers, models, regularizers, optimizers

# ============================================================
# 默认参数
# ============================================================
NUM_SIZE = 0.7
MAX_EPOCHS = 500
INIT_LR = 0.001
L2_REG = 1e-4
LR_DROP_EPOCH = 400
LR_DROP_FACTOR = 0.1
BILSTM_UNITS = 64
CLASS_NAMES = ['Normal', 'FWE', 'FWC', 'RO', 'RL', 'EO', 'NC', 'CF']


def build_cnn_bilstm_model(input_shape, num_classes, bilstm_units=64, l2_reg=1e-4):
    inputs = layers.Input(shape=input_shape)
    x = layers.Conv2D(16, (2, 1), padding='same',
                      kernel_regularizer=regularizers.l2(l2_reg))(inputs)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU()(x)
    x = layers.MaxPooling2D((2, 1), strides=(2, 1))(x)
    x = layers.Conv2D(32, (2, 1), padding='same',
                      kernel_regularizer=regularizers.l2(l2_reg))(x)
    x = layers.BatchNormalization()(x)
    x = layers.ReLU()(x)
    x = layers.Reshape((-1, 32))(x)
    x = layers.Bidirectional(
        layers.LSTM(bilstm_units, return_sequences=False,
                    kernel_regularizer=regularizers.l2(l2_reg))
    )(x)
    outputs = layers.Dense(num_classes, activation='softmax')(x)
    model = models.Model(inputs, outputs, name='CNN_BiLSTM')
    return model


def lr_schedule_fn(epoch, lr):
    if epoch == LR_DROP_EPOCH:
        return lr * LR_DROP_FACTOR
    return lr


def train_mode(data_path, output_dir):
    """训练模型并保存到 output_dir"""
    os.makedirs(output_dir, exist_ok=True)

    print(f'[INFO] 读取数据: {data_path}')
    res = pd.read_excel(data_path, header=None, skiprows=1).values.astype(np.float64)
    print(f'[INFO] 数据形状: {res.shape}')

    num_class = len(np.unique(res[:, -1]))
    num_dim = res.shape[1] - 1
    print(f'[INFO] 特征维度: {num_dim}, 类别数: {num_class}')

    # 打乱
    np.random.seed(42)
    np.random.shuffle(res)

    # 分层划分
    P_train, P_test = [], []
    T_train, T_test = [], []
    for i in range(1, num_class + 1):
        mid_res = res[res[:, -1] == i, :]
        mid_size = mid_res.shape[0]
        mid_train = round(NUM_SIZE * mid_size)
        P_train.append(mid_res[:mid_train, :-1])
        T_train.append(mid_res[:mid_train, -1])
        P_test.append(mid_res[mid_train:, :-1])
        T_test.append(mid_res[mid_train:, -1])

    P_train = np.vstack(P_train)
    T_train = np.hstack(T_train).astype(int) - 1
    P_test = np.vstack(P_test)
    T_test = np.hstack(T_test).astype(int) - 1

    M, N = P_train.shape[0], P_test.shape[0]
    print(f'[INFO] 训练样本: {M}, 测试样本: {N}')

    # 归一化
    scaler = MinMaxScaler(feature_range=(0, 1))
    P_train = scaler.fit_transform(P_train)
    P_test = scaler.transform(P_test)

    # 保存 scaler 参数
    np.save(os.path.join(output_dir, 'scaler_min.npy'), scaler.data_min_)
    np.save(os.path.join(output_dir, 'scaler_max.npy'), scaler.data_max_)
    np.save(os.path.join(output_dir, 'num_dim.npy'), np.array([num_dim]))
    np.save(os.path.join(output_dir, 'num_class.npy'), np.array([num_class]))

    # 整形
    p_train = P_train.reshape(M, num_dim, 1, 1).astype(np.float32)
    p_test = P_test.reshape(N, num_dim, 1, 1).astype(np.float32)

    # 构建模型
    model = build_cnn_bilstm_model((num_dim, 1, 1), num_class, BILSTM_UNITS, L2_REG)
    model.compile(
        optimizer=optimizers.Adam(learning_rate=INIT_LR),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )

    lr_callback = tf.keras.callbacks.LearningRateScheduler(lr_schedule_fn, verbose=0)

    print('[INFO] 开始训练...')
    history = model.fit(
        p_train, T_train,
        epochs=MAX_EPOCHS,
        batch_size=32,
        shuffle=True,
        callbacks=[lr_callback],
        validation_split=0.0,
        verbose=1
    )

    # 保存模型
    model_path = os.path.join(output_dir, 'cnn_bilstm_model.h5')
    model.save(model_path)
    print(f'[INFO] 模型已保存: {model_path}')

    # 评估
    pred_test_prob = model.predict(p_test)
    T_sim2 = np.argmax(pred_test_prob, axis=1)
    acc_test = np.mean(T_sim2 == T_test) * 100

    result = {
        'status': 'success',
        'accuracy': round(acc_test, 2),
        'num_dim': num_dim,
        'num_class': num_class,
        'train_samples': M,
        'test_samples': N,
        'model_path': model_path
    }
    print(f'[RESULT] {json.dumps(result)}')
    return result


def infer_single(features, model_dir):
    """单条推理，返回预测类别名称和概率"""
    model_path = os.path.join(model_dir, 'cnn_bilstm_model.h5')
    if not os.path.exists(model_path):
        raise FileNotFoundError(f'模型文件不存在: {model_path}')

    scaler_min = np.load(os.path.join(model_dir, 'scaler_min.npy'))
    scaler_max = np.load(os.path.join(model_dir, 'scaler_max.npy'))
    num_dim = int(np.load(os.path.join(model_dir, 'num_dim.npy'))[0])
    num_class = int(np.load(os.path.join(model_dir, 'num_class.npy'))[0])

    model = tf.keras.models.load_model(model_path, compile=False)

    # 特征预处理 — 如果传入的特征多了，只取前 num_dim 个（最后一列可能是标签）
    feat = np.array(features, dtype=np.float64).reshape(1, -1)
    if feat.shape[1] < num_dim:
        raise ValueError(f'特征维度不足: 期望至少 {num_dim}, 实际 {feat.shape[1]}')
    feat = feat[:, :num_dim]  # 只取前 num_dim 个特征

    # MinMax 归一化
    feat_norm = (feat - scaler_min) / (scaler_max - scaler_min + 1e-10)

    # 整形
    feat_reshaped = feat_norm.reshape(1, num_dim, 1, 1).astype(np.float32)

    # 预测
    prob = model.predict(feat_reshaped, verbose=0)[0]
    pred_class = int(np.argmax(prob))
    class_name = CLASS_NAMES[pred_class] if pred_class < len(CLASS_NAMES) else f'Class_{pred_class}'

    result = {
        'status': 'success',
        'pred_class': pred_class,
        'class_name': class_name,
        'probability': float(prob[pred_class]),
        'probabilities': [float(p) for p in prob]
    }
    return result


def main():
    parser = argparse.ArgumentParser(description='CNN+BiLSTM 故障诊断')
    parser.add_argument('--mode', required=True, choices=['train', 'infer'])
    parser.add_argument('--data', help='训练数据 Excel 文件路径')
    parser.add_argument('--output_dir', default='.', help='模型输出目录')
    parser.add_argument('--features', help='推理特征，逗号分隔')
    parser.add_argument('--model_dir', default='.', help='模型目录')
    args = parser.parse_args()

    if args.mode == 'train':
        if not args.data:
            print('[ERROR] 训练模式需要 --data 参数', file=sys.stderr)
            sys.exit(1)
        result = train_mode(args.data, args.output_dir)
        sys.exit(0 if result['status'] == 'success' else 1)

    elif args.mode == 'infer':
        if not args.features:
            print('[ERROR] 推理模式需要 --features 参数', file=sys.stderr)
            sys.exit(1)
        features = [float(x.strip()) for x in args.features.split(',')]
        result = infer_single(features, args.model_dir)
        print(f'[RESULT] {json.dumps(result)}')
        sys.exit(0 if result['status'] == 'success' else 1)


if __name__ == '__main__':
    main()
